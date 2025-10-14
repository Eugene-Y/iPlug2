#include <IControls.h>
#include "IPlugEffect.hpp"
#include <hvoya/utils/host_info_UI_tags.hpp>
#include <hvoya/utils/midi_cc/control_decorator.hpp>


static_assert (PLUG_HEIGHT_NO_HEADER + PLUG_INFO_HEADER_HEIGHT == PLUG_HEIGHT,
               "review the sizes in config.h");


inline bool isDebug() {
    #ifdef DEBUG
        return 1;
    #else
        return 0;
    #endif
}


const IColor colDarkGray  (255, 60, 60, 60);
const IColor colLightGray (255, 240, 240, 240);

const IText largeTxtBlack = IText (22, COLOR_BLACK,      ROBOTO_MONO_FN, EAlign::Near, EVAlign::Bottom);
const IText largeTxtLight = IText (22, COLOR_LIGHT_GRAY, ROBOTO_MONO_FN, EAlign::Near, EVAlign::Bottom);

const IText mediumTxtBlack = IText (20, COLOR_BLACK,      ROBOTO_MONO_FN, EAlign::Near, EVAlign::Bottom);
const IText mediumTxtLight = IText (20, COLOR_LIGHT_GRAY, ROBOTO_MONO_FN, EAlign::Near, EVAlign::Bottom);

const IText smallTxtBlack = IText (16, COLOR_BLACK,      ROBOTO_MONO_FN, EAlign::Near, EVAlign::Bottom);
const IText smallTxtLight = IText (16, COLOR_LIGHT_GRAY, ROBOTO_MONO_FN, EAlign::Near, EVAlign::Bottom);


const size_t v_major = IPlugEffect::vMajor (PLUG_VERSION_HEX);
const size_t v_minor = IPlugEffect::vMinor (PLUG_VERSION_HEX);
const size_t v_patch = IPlugEffect::vPatch (PLUG_VERSION_HEX);


auto getVersionColor = [](){
    float hue = v_major * 0.71 + v_minor * 0.21 + v_patch * 0.047;
    while (hue > 1) hue -= 1;
    return IColor().FromHSLA (hue, 0.4, 0.4);
};


auto getDefaultKnobStyle = [](){
    IVStyle s;
    const int w = 240;
    s.colorSpec.mColors [kFR] = IColor (255, w, w, w);
    s.colorSpec.mColors [kX1] = IColor (255, w, w, w);
    s.colorSpec.mColors [kFG] = IColor (0);
    s.colorSpec.mColors [kHL] = IColor (255, 100, 100, 100);
    s.colorSpec.mColors [kPR] = getVersionColor();
    s.drawShadows = false;
    s.widgetFrac = 1;
    IText txt (16, COLOR_WHITE, ROBOTO_MONO_FN);
    txt.mVAlign = EVAlign::Bottom;
    s.labelText = txt;
    s.valueText = txt;
    return s;
};


void IPlugEffect::initializeLayout() {
    #if IPLUG_EDITOR // http://bit.ly/2S64BDd

        static constexpr float boundsPad = 10;
        static constexpr float headerH = PLUG_INFO_HEADER_HEIGHT;

        const auto getFullBounds = [=] (IGraphics* pG, float pad = boundsPad) {
            return pG->GetBounds().GetPadded (-pad);
        };

        const auto getUIBounds = [=] (IGraphics* pG, float pad = boundsPad) {
            return pG->GetBounds().GetReducedFromTop (headerH).GetPadded (-pad);
        };

        #pragma mark header layout

        const auto createHeaderLayout = [&](IGraphics* pG) {
          const IRECT headerR = getFullBounds (pG, 0).GetFromTop (headerH);

            pG->AttachControl (new IPanelControl (headerR, getVersionColor()));

            const ISVG logo = pG->LoadSVG (HVOYA_LOGO_HANDLE_FN);
            pG->AttachControl (new ISVGControl (headerR.GetPadded (-10), logo));

            const auto stringRect = [=] (int row, int col) {
                return headerR.SubRectVertical (8, row)
                              .GetVPadded (2)
                              .GetTranslated (headerH + 160 * col, 11);
            };

            auto plugNameTxt = largeTxtLight;
            plugNameTxt.mVAlign = EVAlign::Middle;
			std::string str = std::format ("{} v{} {}", PLUG_NAME, PLUG_VERSION_STR,
										   (isDebug() ? "DEBUG" : "RELEASE"));
			pG->AttachControl (new ITextControl (stringRect (0, 0).GetVPadded (5),
												 str.c_str(), plugNameTxt));

			str = std::format ("{} {}", PLUG_PRODUCT, PLUG_ARCHS);
			pG->AttachControl (new ITextControl (stringRect (2, 0),  str.c_str(), smallTxtLight));

			pG->AttachControl (new ITextControl (stringRect (3, 0), PLUG_GIT_BRANCH_NAME, smallTxtLight));
			pG->AttachControl (new ITextControl (stringRect (4, 0), PLUG_GIT_COMMIT_SHA, smallTxtLight));

			pG->AttachControl (new ITextControl (stringRect (5, 0), PLUG_BUILD_DATE, smallTxtLight));

            using namespace hvoya;
            pG->AttachControl (new ITextControl (stringRect (2, 1), "sr:         ---",
                                                 smallTxtLight), tag_SampleRate);
            pG->AttachControl (new ITextControl (stringRect (3, 1), "buf size:   ---",
                                                 smallTxtLight), tag_BufSize);
            pG->AttachControl (new ITextControl (stringRect (4, 1), "I/O:        ---",
                                                 smallTxtLight), tag_Chans);
            pG->AttachControl (new ITextControl (stringRect (5, 1),  "host time: ---",
                                                 smallTxtLight), tag_HostPos);

			auto style = getDefaultKnobStyle();
			style.colorSpec.mColors [kFR] = COLOR_TRANSPARENT;
			auto br = headerR.GetFromTRHC (25, 18);
			pG->AttachControl (new IVButtonControl(br,
				[](IControl* pC) {
					auto pG = pC->GetUI();
					bool show = pG->ShowControlBoundsEnabled();
					pG->ShowControlBounds (!show);
				},"[ ]", style));
        };


        #pragma mark main UI layout

        mLayoutFunc = [&](IGraphics* pG) {
            pG->LoadFont ("Roboto-Regular", ROBOTO_FN);
            pG->LoadFont ("RobotoMono-Regular.ttf", ROBOTO_MONO_FN);

            pG->AttachCornerResizer (EUIResizerMode::Scale, false);
            pG->AttachPanelBackground (colDarkGray);
            pG->EnableMouseOver (true);
            pG->EnableTooltips (true);

            createHeaderLayout (pG);
            
            [[maybe_unused]] const auto knobStyle = getDefaultKnobStyle();

            const IRECT b = getUIBounds (pG, 0);
            IControl* pC;
            IRECT r;
			IVStyle s;

			auto btnNoRectStyle = knobStyle;
			btnNoRectStyle.colorSpec.mColors [kFR] = COLOR_TRANSPARENT;

            pC = new ITextControl (b.GetCentredInside (b.W(), b.H()/3).GetVShifted (-b.H()/4), "Hello IPlugEffect!", IText (50, COLOR_LIGHT_GRAY, ROBOTO_MONO_FN));
            pG->AttachControl (pC);

            using namespace hvoya;

            r = b.GetCentredInside (.75 * b.W(), b.H()/2).GetVShifted (b.H()/6);

            pC = _midiCCMediator.createLearnable <IVKnobControl> (r.SubRectHorizontal (3, 0), par_lim_thresh, "lim thresh", knobStyle);
            pG->AttachControl (pC);
            
            pC = _midiCCMediator.createLearnable <IVKnobControl> (r.SubRectHorizontal (3, 1), par_lim_softness, "lim softness", knobStyle);
            pG->AttachControl (pC);
            
            pC = _midiCCMediator.createLearnable <IVKnobControl> (r.SubRectHorizontal (3, 2), par_master_mix, "master mix", knobStyle);
            pG->AttachControl (pC);

		#pragma mark about box

			const int cTagAbout = 7;

			auto textStyle = smallTxtLight;
			textStyle.mVAlign = EVAlign::Middle;
			textStyle.mAlign = EAlign::Center;


			pC = new IVButtonControl (b.GetFromTRHC (20, 18), SplashClickActionFunc, "i", btnNoRectStyle);
			pC->SetAnimationEndActionFunction ([pG] (IControl* pC) {
				pG->GetControlWithTag (cTagAbout)->As<IAboutBoxControl>()->Show();
			});
			pG->AttachControl(pC);

			auto aboutAttachFunc = [textStyle] (IContainerBase* pP, const IRECT& r) {
				pP->AddChildControl (new IURLControl (IRECT(), "hvoya.audio", PLUG_URL_STR, textStyle));
				std::string str = std::format ("{} v{}", PLUG_NAME, PLUG_VERSION_STR);
				pP->AddChildControl (new ITextControl (IRECT(), str.c_str(), textStyle));
				str = std::format ("{} {} {}", PLUG_PRODUCT, PLUG_ARCHS, (isDebug() ? "debug" : "release"));
				pP->AddChildControl (new ITextControl (IRECT(), str.c_str(), textStyle));
				str = std::format ("{} {}", PLUG_GIT_BRANCH_NAME, PLUG_GIT_COMMIT_SHA);
				pP->AddChildControl (new ITextControl (IRECT(), str.c_str(), textStyle));
				pP->AddChildControl (new ITextControl (IRECT(), PLUG_BUILD_DATE, textStyle));
				pP->Hide (true);
			};

			auto aboutResizeFunc = [textStyle](IContainerBase* pP, const IRECT& rect) {
				const auto nc = pP->NChildren();
				auto r = rect.GetMidVPadded (1.2 * nc / 2 * textStyle.mSize);
				pP->ForAllChildrenFunc ([nc, r](int i, IControl* pC) {
					pC->SetTargetAndDrawRECTs (r.SubRectVertical (nc, i));
				});
			};

			pC = new IAboutBoxControl (b, colDarkGray, aboutAttachFunc, aboutResizeFunc);
			pC->Hide (true);
			pG->AttachControl (pC, cTagAbout);

            _midiCCMediator.UpdateMidiControllableUI();
        };

    #endif
}


void IPlugEffect::updateHostInfoView() {
    const auto& m = _hostInfoModel;
    const auto i = m.getInfo();
    auto& v = _hostInfoView;
    bool updOk = true;
    if (m.getUpdateHostPosition())
        updOk &= v.updateHostPositionView (i.hostTime);
        
    if (updOk && m.getUpdateSampleRate())
        updOk &= v.updateSampleRateView (i.sampleRate);
        
    if (updOk && m.getUpdateBufSize())
        updOk &= v.updateBufSizeView (i.bufSize);
    
    if (updOk && m.getUpdateChans())
        updOk &= v.updateChansView (i.chanIn, i.chanOut);
        
    if (updOk)
        _hostInfoModel.clearUpdateFlags();
}
