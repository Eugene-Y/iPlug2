#include <sstream>
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

        static std::stringstream sstream;
        sstream.precision (3);

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
            sstream.str (std::string());
            sstream << PLUG_NAME << "  v" << PLUG_VERSION_STR << "  " << (isDebug() ? "DEBUG" : "RELEASE");
            pG->AttachControl (new ITextControl (stringRect (0, 0).GetVPadded (5),
                                                 sstream.str().c_str(), plugNameTxt));

            sstream.str (std::string());
            sstream << PLUG_PRODUCT << " " << PLUG_ARCHS;
            pG->AttachControl (new ITextControl (stringRect (2, 0), sstream.str().c_str(), smallTxtLight));

            pG->AttachControl (new ITextControl (stringRect (3, 0), PLUG_GIT_BRANCH_NAME, smallTxtLight));
            pG->AttachControl (new ITextControl (stringRect (4, 0), PLUG_GIT_COMMIT_SHA, smallTxtLight));

            sstream.str (std::string());
            sstream << PLUG_BUILD_DATE;
            pG->AttachControl (new ITextControl (stringRect (5, 0), sstream.str().c_str(), smallTxtLight));

            using namespace hvoya;
            pG->AttachControl (new ITextControl (stringRect (2, 1), "sr:         ---",
                                                 smallTxtLight), tag_SampleRate);
            pG->AttachControl (new ITextControl (stringRect (3, 1), "buf size:   ---",
                                                 smallTxtLight), tag_BufSize);
            pG->AttachControl (new ITextControl (stringRect (4, 1), "I/O:        ---",
                                                 smallTxtLight), tag_Chans);
            pG->AttachControl (new ITextControl (stringRect (5, 1),  "host time: ---",
                                                 smallTxtLight), tag_HostPos);
        };


        #pragma mark main UI layout

        mLayoutFunc = [&](IGraphics* pG) {
            pG->LoadFont ("Roboto-Regular", ROBOTO_FN);
            pG->LoadFont ("RobotoMono-Regular.ttf", ROBOTO_MONO_FN);

            pG->AttachCornerResizer (EUIResizerMode::Scale, false);
            pG->AttachPanelBackground (colDarkGray);
            pG->EnableMouseOver (true);
            pG->EnableTooltips (true);
            //pG->ShowControlBounds (true);

            createHeaderLayout (pG);
            
            [[maybe_unused]] const auto knobStyle = getDefaultKnobStyle();

            const IRECT b = getUIBounds (pG, 0);
            IControl* pC;

            IRECT r;

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
