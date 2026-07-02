#pragma once

// midi_cc_mapper.hpp

#include <map>
#include <vector>
#include <array>
#include <algorithm>
#include <atomic>

#include <hvoya/utils/log/logger.hpp>
#include <hvoya/utils/types.hpp>
#include "controllable.hpp"




namespace hvoya::midi_cc {
    
    struct ParamCCMapping {
        PId_t  paramId;
        CC_t   cc;
        double minVal   { 0.0 };
        double maxVal   { 1.0 };
        int    channel  { 0 };   // 0 = all channels, 1-16 = specific (1-indexed)

        ParamCCMapping (PId_t id = uninit::pid, CC_t cc = uninit::cc)
          : paramId (id), cc (cc) {}

        double mapVal (double normalized) const {
            double mapped = normalized;
            mapped = minVal + mapped * (maxVal - minVal);
            assert (mapped >= 0 && mapped <= 1);
            return mapped;
        }

        bool channelMatches (int midiChannel1Indexed) const {
            return channel == 0 || channel == midiChannel1Indexed;
        }
    };
    
    
    typedef std::vector <ParamCCMapping> ParamCCMappings_t;
    typedef std::map <CC_t, ParamCCMappings_t> CCtoParamMap_t;
    

    class Mapper {
        
        public:

            struct ResolvedParam {
                const PId_t id;
                const double mappedNormalizedVal;
                ResolvedParam (PId_t id, double v) : id (id), mappedNormalizedVal (v) {}
            };
            
            void setListeningParamId (PId_t i) { _listeningPId = i; }
            // Arm learn for a param. mostMoved=false (default, unchanged for existing callers): the FIRST
            // incoming CC binds. mostMoved=true: bind the first CC that MOVES past a threshold — so a 2D
            // pad (which emits both axis CCs on any touch) binds the axis you actually sweep, not whichever
            // message arrived first. Used for the morph pad's per-axis X/Y learn.
            void setLearningForParam (PId_t, bool mostMoved = false);

            // Swap which CC drives each of two params (their bindings' paramId), keeping each binding's
            // cc/range/channel. For the morph pad's "Swap X/Y" when a pad's axes come out reversed.
            void swapParamMappings (PId_t, PId_t);

            // Abort an armed learn (no CC arrived) without binding anything.
            void cancelLearning() { _isLearning = false; }
            bool isLearning() const { return _isLearning; }
            // The param currently armed for learn, or uninit::pid when not learning — lets the UI
            // blink that control's presence dot as an "I'm listening" signal.
            PId_t learningParamId() const { return _isLearning ? _listeningPId : uninit::pid; }

            // True once a CC was bound on the audio thread (learn completed). The UI thread
            // consumes this to refresh control CC# indicators — the audio thread must never
            // touch UI controls (they may have been destroyed by a layout rebuild).
            bool takeUIDirty() { return _ccMapDirtyForUI.exchange (false); }
            
            void setMaxForListeningParam (double);
            void setMinForListeningParam (double);
            void invertRangeForListeningParam();
            
            void clearAllMappings ();
            void clearMappingForParam (PId_t);

            // midiChannel: 1-indexed (1-16); pass 0 to skip channel filtering
            std::vector <ResolvedParam> processMidiCC (CC_t, double normVal, int midiChannel = 0);

            // Returns the CC number mapped to the given param, or uninit::cc if none.
            int getCCForParam (PId_t) const;

            // Sets the channel filter for an existing mapping (0=all, 1-16=specific).
            void setChannelForParam (PId_t, int channel);

            const CCtoParamMap_t& getCCtoParamMap() const { return _ccToParamMap; }
            void setCCtoParamMap (CCtoParamMap_t&& map) { _ccToParamMap = std::move (map); }
            
        private:

            PId_t _listeningPId = uninit::pid;
            std::atomic<bool> _isLearning { false };       // armed on the message thread, cleared on audio
            std::atomic<bool> _ccMapDirtyForUI { false };  // set on audio when a CC binds; UI thread refreshes
            CCtoParamMap_t _ccToParamMap;

            // "Most-moved" learn state (mostMoved=true). Fixed POD arrays (no alloc) indexed by CC: the
            // first value seen per CC this learn window, so we can bind the CC that moves past the
            // threshold. Cleared on arm (message thread) / touched on the audio drain — a benign POD race
            // (same tolerance as _listeningPId), no structural mutation → no crash.
            static constexpr int    kNumCCs          = 128;
            static constexpr double kLearnMoveThresh = 0.12;   // ~15/127 of the CC range
            bool   _learnMostMoved = false;
            std::array<double, kNumCCs> _learnFirstVal {};
            std::array<bool,   kNumCCs> _learnSeen {};
        
            void addMapping (CC_t, PId_t);
            ParamCCMapping* findMappingForPId (PId_t);
            
        };

} // namespace hvoya
