#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include <hvoya/utils/utils.hpp>


namespace hvoya {


// Glides between MIDI note numbers in note-space and converts to Hz at output.
// Lives entirely on the audio thread — no thread safety.
//
// Holds a stack of currently-pressed notes (press order) so monophonic note
// priority behaves like a real glide instrument:
//   - note on        : target = the new note; when several notes land in the SAME
//                       sub-block (≈ a simultaneous chord) the nearest of them (by
//                       real, shift-applied target) to the current pitch wins.
//   - note off        : fall back to the most-recently-pressed note still held (stack
//                       top); when the last note is released the glide freezes in place.
//   - glide trigger   : legato → only when a note was already held (overlap); always →
//                       on every new target. glideMs == 0 snaps. A new target mid-glide
//                       restarts the glide from the current pitch.
template <typename T>
struct NoteGlider {

    // Call when note mode activates. Sets starting position from knob Hz via freqToNote.
    void activate (T initHz) {
        _curNote          = utils::freqToNote (initHz);
        _tgtNote          = _curNote;
        _targetNote       = static_cast<int> (std::lround (_curNote));
        _totalSubBlks     = T (0);
        _elapsed          = T (0);
        _heldCount        = 0;       // clear the held-note stack
        _targetSetThisBlk = false;
    }

    // midiNote    : MIDI note number (0-127)
    // octShift    : octaves offset
    // semiShift   : semitones offset
    // centShift   : cents offset (integer cents, e.g. 50 = 0.5 semitone)
    // glideMs     : glide time in milliseconds
    // osSampleRate: oversampled sample rate (hostSR * osFactor)
    // smallBlkSz  : sub-block size in OS samples (SMALL_BLOCK_SIZE)
    // legatoOnly  : if true, glide only when a previous note is still held
    void noteOn (int midiNote, int octShift, int semiShift, int centShift,
                 T glideMs, T osSampleRate, T smallBlkSz, bool legatoOnly) {
        // Cache the shifts + glide params so a later note-off fallback can re-target
        // (it gets no params of its own).
        _octShift = octShift; _semiShift = semiShift; _centShift = centShift;
        _glideMs  = glideMs;  _osSR      = osSampleRate; _smallBlk = smallBlkSz;

        if (_contains (midiNote)) return;        // retrigger of an already-held note → ignore
        const bool wasHeld = _heldCount > 0;     // overlap = something was already down
        _push (midiNote);

        const T cand = _computeTarget (midiNote, octShift, semiShift, centShift);

        // Nearest-of-chord: if another note already became the target THIS sub-block, keep
        // whichever target is nearer the current pitch (so a simultaneous chord glides to
        // its closest note). The first arrival of the block always wins outright.
        if (_targetSetThisBlk) {
            if (std::abs (cand - _curNote) >= std::abs (_tgtNote - _curNote))
                return;                          // existing target nearer → new note just joins the stack
        } else {
            _targetSetThisBlk = true;
        }

        _targetNote = midiNote;
        const bool doGlide = (!legatoOnly || wasHeld) && glideMs > T (0);
        _applyTarget (cand, doGlide);
    }

    // Release one note. Removes it from the stack; if notes remain, glide to the most
    // recently pressed survivor (stack top). When the stack empties, freeze in place.
    void noteOff (int midiNote) {
        _remove (midiNote);
        if (_heldCount == 0) {
            _tgtNote      = _curNote;            // freeze the glide at the current value
            _totalSubBlks = T (0);
            _elapsed      = T (0);
            return;
        }
        const int top = _held[_heldCount - 1];   // most recent still-held note
        _targetNote   = top;
        const T target = _computeTarget (top, _octShift, _semiShift, _centShift);
        _applyTarget (target, _glideMs > T (0)); // overlap → glide (snap if no glide time)
    }

    // Called when shift params change. Updates target immediately; glide continues.
    void updateShifts (int octShift, int semiShift, int centShift) {
        _octShift = octShift; _semiShift = semiShift; _centShift = centShift;
        _tgtNote  = _computeTarget (_targetNote, octShift, semiShift, centShift);
        if (_totalSubBlks <= T (0))
            _curNote = _tgtNote;
    }

    // Called when glide time changes mid-glide. Adjusts remaining time proportionally.
    // New remaining = max(0, newTotal - elapsed). If zero, snaps to target.
    void updateGlideMs (T glideMs, T osSampleRate, T smallBlkSz) {
        _glideMs = glideMs; _osSR = osSampleRate; _smallBlk = smallBlkSz;  // cache for note-off fallback
        if (_totalSubBlks <= T (0)) return;
        const T newTotal = glideMs / T (1000) * osSampleRate / smallBlkSz;
        if (newTotal <= _elapsed) {
            _curNote      = _tgtNote;
            _totalSubBlks = T (0);
            _elapsed      = T (0);
        } else {
            _totalSubBlks = newTotal;
        }
    }

    // CC smooth duration in sub-blocks. Call whenever sample rate or OS factor changes.
    void setCCSmoothBlks (T blks) { _ccSmoothBlks = blks > T (0) ? blks : T (0); }

    // CC offset in note units, applied to the output (not the target).
    // Ramps to the new value over _ccSmoothBlks sub-blocks to prevent zipper noise.
    void setCCOffset (T offset) {
        if (offset == _tgtCCOffset) return;
        _tgtCCOffset       = offset;
        _ccSmoothRemaining = _ccSmoothBlks;
    }

    // Advance one sub-block and return current Hz (including CC offset).
    T tick () {
        if (_totalSubBlks > T (0)) {
            if (_elapsed < _totalSubBlks) {
                const T remaining = _totalSubBlks - _elapsed;
                _curNote += (_tgtNote - _curNote) / remaining;
                _elapsed += T (1);
            }
            if (_elapsed >= _totalSubBlks || std::abs (_curNote - _tgtNote) < T (1e-5)) {
                _curNote      = _tgtNote;
                _totalSubBlks = T (0);
                _elapsed      = T (0);
            }
        }
        if (_ccSmoothRemaining > T (0)) {
            _ccOffset += (_tgtCCOffset - _ccOffset) / _ccSmoothRemaining;
            _ccSmoothRemaining -= T (1);
            if (_ccSmoothRemaining <= T (0))
                _ccOffset = _tgtCCOffset;
        }
        // End of sub-block: the next block's note-ons start a fresh nearest-of-chord round.
        _targetSetThisBlk = false;
        return utils::noteToFreq (_curNote + _ccOffset);
    }

    T    getCurrentHz ()   const { return utils::noteToFreq (_curNote + _ccOffset); }
    T    getCurrentNote () const { return _curNote; }
    bool isNoteActive ()   const { return _heldCount > 0; }

private:

    // Set a new glide target. doGlide=false snaps; doGlide=true (re)starts the ramp from
    // the current pitch over the full glide time (a target change mid-glide restarts it).
    void _applyTarget (T target, bool doGlide) {
        if (doGlide) {
            _tgtNote      = target;
            _totalSubBlks = _glideMs / T (1000) * _osSR / _smallBlk;
            _elapsed      = T (0);
        } else {
            _curNote      = target;
            _tgtNote      = target;
            _totalSubBlks = T (0);
            _elapsed      = T (0);
        }
    }

    // ── Held-note stack (press order; top = most recent). Set semantics: a note number
    //    appears at most once. RT-safe — fixed storage, no allocation. ───────────────
    static constexpr int kMaxHeld = 128;   // MIDI note count — can't hold more distinct notes

    bool _contains (int n) const {
        for (int i = 0; i < _heldCount; ++i) if (_held[i] == n) return true;
        return false;
    }
    void _push (int n) {
        if (_heldCount < kMaxHeld) _held[_heldCount++] = n;
    }
    void _remove (int n) {
        for (int i = 0; i < _heldCount; ++i)
            if (_held[i] == n) {
                for (int j = i; j < _heldCount - 1; ++j) _held[j] = _held[j + 1];
                --_heldCount;
                return;
            }
    }

    static T _computeTarget (int midiNote, int octShift, int semiShift, int centShift) {
        return T (midiNote) + T (octShift) * T (12) + T (semiShift) + T (centShift) / T (100);
    }

    T    _curNote      = T (69);
    T    _tgtNote      = T (69);
    int  _targetNote   = 69;       // raw MIDI note of the current target (for shift recompute / fallback)
    T    _totalSubBlks = T (0);
    T    _elapsed      = T (0);

    T    _ccOffset          = T (0);
    T    _tgtCCOffset       = T (0);
    T    _ccSmoothBlks      = T (8);
    T    _ccSmoothRemaining = T (0);

    std::array<int, kMaxHeld> _held {};
    int  _heldCount = 0;

    // Cached from the last noteOn / updateGlideMs so note-off fallback re-targets correctly.
    int  _octShift = 0, _semiShift = 0, _centShift = 0;
    T    _glideMs = T (0), _osSR = T (0), _smallBlk = T (1);

    // True once a target was chosen in the current sub-block (nearest-of-chord arbitration).
    bool _targetSetThisBlk = false;
};


} // namespace hvoya
