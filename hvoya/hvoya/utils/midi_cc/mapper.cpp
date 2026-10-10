#include "mapper.hpp"
#include <cmath>
#include <hvoya/utils/log/logger.hpp>


namespace hvoya::midi_cc {
    
    #ifndef MCCM_LOGD
        #define MCCM_LOGD LOGD << "Midi CC Mapper: "
        #define MCCM_LOGW LOGW << "Midi CC Mapper Warning: "
    #else
        #error "rename log macros"
    #endif
    
            
    auto findMappingIteratorForPId (std::vector <ParamCCMapping>& params, PId_t i) {
        return std::find_if (params.begin(), params.end(), 
                [i](const ParamCCMapping& m) { return m.paramId == i; });
    }
            
    
    Mapper::Mapper() {
        std::lock_guard lock (_mutex);
        publishLocked();
    }


    Mapper::~Mapper() {
        delete _rtTable.load();
    }


    void Mapper::setListeningParamId (PId_t i) {
        std::lock_guard lock (_mutex);
        _listeningPId = i;
    }


    void Mapper::setLearningForParam (PId_t i, bool mostMoved) {
        std::lock_guard lock (_mutex);
        clearMappingForParamLocked (i);
        MCCM_LOGD << "learning param " << i;
        _listeningPId   = i;
        _learnMostMoved = mostMoved;
        _learnSeen.fill (false);   // fresh movement window
        publishLocked();
        _isLearning = true;
    }


    void Mapper::swapParamMappings (PId_t a, PId_t b) {
        std::lock_guard lock (_mutex);
        for (auto& [cc, params] : _ccToParamMap)
            for (auto& pm : params) {
                if      (pm.paramId == a) pm.paramId = b;
                else if (pm.paramId == b) pm.paramId = a;
            }
        publishLocked();
        _ccMapDirtyForUI.store (true, std::memory_order_relaxed);
    }


    void Mapper::invertRangeForListeningParam() {
        std::lock_guard lock (_mutex);
        if (auto pM = findMappingForPIdLocked (_listeningPId)) {
            MCCM_LOGD <<  " param " << _listeningPId << " invert range";
            std::swap (pM->minVal, pM->maxVal);
            publishLocked();
        }
    }


    void Mapper::setMaxForListeningParam (double v) {
        assert (v >= 0 && v <= 1);
        std::lock_guard lock (_mutex);
        if (auto pM = findMappingForPIdLocked (_listeningPId)) {
            MCCM_LOGD <<  " param " << _listeningPId << " set max " << v;
            pM->maxVal = v;
            publishLocked();
        }
    }


    void Mapper::setMinForListeningParam (double v) {
        assert (v >= 0 && v <= 1);
        std::lock_guard lock (_mutex);
        if (auto pM = findMappingForPIdLocked (_listeningPId)) {
            MCCM_LOGD <<  " param " << _listeningPId << " set min " << v;
            pM->minVal = v;
            publishLocked();
        }
    }


    void Mapper::clearAllMappings () {
        std::lock_guard lock (_mutex);
        _ccToParamMap.clear();
        _listeningPId = uninit::pid;
        _isLearning   = false;   // an armed learn would otherwise bind the next CC to the uninit pid
        publishLocked();
        MCCM_LOGD << "clear all mappings";
    }


    void Mapper::clearMappingForParam (PId_t i) {
        std::lock_guard lock (_mutex);
        clearMappingForParamLocked (i);
        publishLocked();
    }


    void Mapper::clearMappingForParamLocked (PId_t i) {
        MCCM_LOGD << "clear mapping for param " << i;
        for (auto ccIt = _ccToParamMap.begin(); ccIt != _ccToParamMap.end(); ++ccIt) {
            auto& params = ccIt->second;
            auto it = findMappingIteratorForPId (params, i);
            if (it != params.end()) {
                params.erase (it);
                if (params.empty())
                    _ccToParamMap.erase (ccIt);
                return;
            }
        }
    }


    // Audio thread. Allocates when a CC binds — once per learn, never during normal playback.
    void Mapper::advanceLearning (CC_t cc, double normVal) {
        if (!_isLearning)
            return;
        std::unique_lock lock (_mutex, std::try_to_lock);
        if (!lock.owns_lock() || !_isLearning)
            return;
        if (_listeningPId < 0) {
            _isLearning = false;
            return;
        }
        if (!_learnMostMoved) {
            addMappingLocked (cc, _listeningPId);
            _isLearning = false;
        } else if (cc >= 0 && cc < kNumCCs) {
            // Bind the first CC that MOVES past the threshold from its first-seen value. A 2D pad emits
            // both axis CCs on touch; sweeping one axis makes it cross first → that axis binds.
            if (!_learnSeen[cc]) { _learnSeen[cc] = true; _learnFirstVal[cc] = normVal; }
            else if (std::abs (normVal - _learnFirstVal[cc]) >= kLearnMoveThresh) {
                addMappingLocked (cc, _listeningPId);
                _isLearning = false;
            }
        }
    }


    int Mapper::getCCForParam (PId_t id) const {
        std::lock_guard lock (_mutex);
        for (const auto& [cc, params] : _ccToParamMap)
            for (const auto& pm : params)
                if (pm.paramId == id)
                    return cc;
        return uninit::cc;
    }


    void Mapper::setChannelForParam (PId_t id, int channel) {
        std::lock_guard lock (_mutex);
        for (auto& [cc, params] : _ccToParamMap)
            for (auto& pm : params)
                if (pm.paramId == id) {
                    pm.channel = channel;
                    publishLocked();
                    return;
                }
    }


    CCtoParamMap_t Mapper::getCCtoParamMap() const {
        std::lock_guard lock (_mutex);
        return _ccToParamMap;
    }


    void Mapper::setCCtoParamMap (CCtoParamMap_t&& map) {
        std::lock_guard lock (_mutex);
        _ccToParamMap = std::move (map);
        publishLocked();
    }


    void Mapper::addMappingLocked (CC_t cc, PId_t i) {
        MCCM_LOGD << "add mapping CC " << cc << " -> param " << i;
        auto& params = _ccToParamMap [cc];
        auto it = findMappingIteratorForPId (params, i);
        if (it == params.end()) {
            params.emplace_back (ParamCCMapping (i, cc));
        }
        publishLocked();
        // Don't touch UI controls here — addMapping runs on the audio thread and the
        // control may have been destroyed by a layout rebuild while learn was armed
        // (that dangling setCCNumber() was a crash). Flag for the UI thread to refresh.
        _ccMapDirtyForUI.store (true, std::memory_order_relaxed);
    }


    ParamCCMapping* Mapper::findMappingForPIdLocked (PId_t i) {
        for (auto& [cc, params] : _ccToParamMap) {
            auto it = findMappingIteratorForPId (params, i);
            if (it != params.end()) {
                return &(*it);
            }
        }
        MCCM_LOGW << "no param " << i;
        return nullptr;
    }


    // Hazard-pointer read (single reader): pin the table, then confirm it is still the published one —
    // if the writer swapped it in between, it may already be retired, so pin the new one instead.
    // seq_cst on both sides is what makes "writer sees the pin" or "reader sees the swap" certain.
    const Mapper::RTTable* Mapper::acquireRTTable() {
        const RTTable* t = _rtTable.load (std::memory_order_seq_cst);
        for (;;) {
            _rtTableInUse.store (t, std::memory_order_seq_cst);
            const RTTable* published = _rtTable.load (std::memory_order_seq_cst);
            if (published == t)
                return t;
            t = published;
        }
    }


    void Mapper::publishLocked() {
        auto table = std::make_unique<RTTable>();
        for (CC_t cc = 0; cc < kNumCCs; ++cc) {
            table->ccBegin [cc] = uint32_t (table->mappings.size());
            if (const auto it = _ccToParamMap.find (cc); it != _ccToParamMap.end())
                table->mappings.insert (table->mappings.end(), it->second.begin(), it->second.end());
        }
        table->ccBegin [kNumCCs] = uint32_t (table->mappings.size());

        if (const RTTable* replaced = _rtTable.exchange (table.release(), std::memory_order_seq_cst))
            _retiredRTTables.emplace_back (replaced);
        const RTTable* inUse = _rtTableInUse.load (std::memory_order_seq_cst);
        std::erase_if (_retiredRTTables, [inUse] (const auto& t) { return t.get() != inUse; });
    }


    #undef MCCM_LOGD
    #undef MCCM_LOGW

} // namespace hvoya
