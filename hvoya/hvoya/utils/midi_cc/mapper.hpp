#pragma once

// midi_cc_mapper.hpp

#include <map>
#include <vector>
#include <array>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>

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
    

    // Threading. The map is edited on the message/host threads under _mutex; the audio thread never
    // reads it. After every edit the writer publishes an immutable flat copy (RTTable), and
    // processMidiCC — the only audio-thread entry, from ONE thread — reads that copy lock-free,
    // pinning it with a single hazard pointer so the writer frees a replaced table only once the
    // audio thread is off it. Learn completes on the audio thread, but only via try_lock: when the
    // map is busy that message just doesn't count and the next CC binds.
    class Mapper {

        public:

            Mapper();
            ~Mapper();

            void setListeningParamId (PId_t);
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

            // Audio thread. Calls onMapped (paramId, mappedNormalizedVal) for every param the CC drives.
            // midiChannel: 1-indexed (1-16); pass 0 to skip channel filtering
            template <typename OnMapped>
            void processMidiCC (CC_t cc, double normVal, int midiChannel, OnMapped&& onMapped) {
                advanceLearning (cc, normVal);   // first, so a CC learned by this message already drives
                if (cc < 0 || cc >= kNumCCs)
                    return;
                const RTTableReader reader (*this);
                const RTTable& table = reader.table();
                for (auto i = table.ccBegin [cc]; i < table.ccBegin [cc + 1]; ++i) {
                    const auto& pM = table.mappings [i];
                    if (midiChannel == 0 || pM.channelMatches (midiChannel))
                        onMapped (pM.paramId, pM.mapVal (normVal));
                }
            }

            // Returns the CC number mapped to the given param, or uninit::cc if none.
            int getCCForParam (PId_t) const;

            // Sets the channel filter for an existing mapping (0=all, 1-16=specific).
            void setChannelForParam (PId_t, int channel);

            CCtoParamMap_t getCCtoParamMap() const;   // a copy: the live map is guarded
            void setCCtoParamMap (CCtoParamMap_t&& map);

        private:

            static constexpr int    kNumCCs          = 128;
            static constexpr double kLearnMoveThresh = 0.12;   // ~15/127 of the CC range

            // The audio thread's view: every mapping, grouped by CC; CC c owns [ccBegin[c], ccBegin[c+1]).
            struct RTTable {
                std::array<uint32_t, kNumCCs + 1> ccBegin {};
                std::vector<ParamCCMapping>       mappings;
            };

            class RTTableReader {
                public:
                    explicit RTTableReader (Mapper& m) : _m (m), _t (m.acquireRTTable()) {}
                    ~RTTableReader() { _m._rtTableInUse.store (nullptr, std::memory_order_release); }
                    RTTableReader (const RTTableReader&) = delete;
                    RTTableReader& operator= (const RTTableReader&) = delete;
                    const RTTable& table() const { return *_t; }
                private:
                    Mapper&        _m;
                    const RTTable* _t;
            };

            mutable std::mutex _mutex;   // guards everything below up to the RT table
            PId_t _listeningPId = uninit::pid;
            CCtoParamMap_t _ccToParamMap;
            // "Most-moved" learn state (mostMoved=true): the first value seen per CC this learn window, so
            // we can bind the CC that moves past the threshold.
            bool   _learnMostMoved = false;
            std::array<double, kNumCCs> _learnFirstVal {};
            std::array<bool,   kNumCCs> _learnSeen {};
            std::vector<std::unique_ptr<const RTTable>> _retiredRTTables;

            std::atomic<bool> _isLearning { false };       // armed on the message thread, cleared on audio
            std::atomic<bool> _ccMapDirtyForUI { false };  // set on audio when a CC binds; UI thread refreshes
            std::atomic<const RTTable*> _rtTable { nullptr };
            std::atomic<const RTTable*> _rtTableInUse { nullptr };   // the audio thread's hazard pointer

            const RTTable* acquireRTTable();
            void publishLocked();
            void advanceLearning (CC_t, double normVal);
            void addMappingLocked (CC_t, PId_t);
            void clearMappingForParamLocked (PId_t);
            ParamCCMapping* findMappingForPIdLocked (PId_t);
        };

} // namespace hvoya
