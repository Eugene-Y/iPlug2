#pragma once

#include <cmath>
#include <algorithm>
#include <hvoya/utils/utils.hpp>


namespace hvoya {


// Glides between MIDI note numbers in note-space and converts to Hz at output.
// Lives entirely on the audio thread — no thread safety.
template <typename T>
struct NoteGlider {

    // Call when note mode activates. Sets starting position from knob Hz via freqToNote.
    void activate (T initHz) {
        _curNote      = utils::freqToNote (initHz);
        _tgtNote      = _curNote;
        _totalSubBlks = T (0);
        _elapsed      = T (0);
        _noteActive   = false;
    }

    // midiNote    : MIDI note number (0-127)
    // octShift    : octaves offset
    // semiShift   : semitones offset
    // centShift   : cents offset (integer cents, e.g. 50 = 0.5 semitone)
    // glideMs     : glide time in milliseconds
    // osSampleRate: oversampled sample rate (hostSR * osFactor)
    // smallBlkSz  : sub-block size in OS samples (SMALL_BLOCK_SIZE)
    // legatoOnly  : if true, glide only when a previous note is still active
    void noteOn (int midiNote, int octShift, int semiShift, int centShift,
                 T glideMs, T osSampleRate, T smallBlkSz, bool legatoOnly) {
        const bool wasActive = _noteActive;
        _noteActive    = true;
        _lastMidiNote  = midiNote;

        const T newTarget = _computeTarget (midiNote, octShift, semiShift, centShift);
        const bool doGlide = (!legatoOnly || wasActive) && glideMs > T (0);

        if (doGlide) {
            _tgtNote      = newTarget;
            _totalSubBlks = glideMs / T (1000) * osSampleRate / smallBlkSz;
            _elapsed      = T (0);
        } else {
            _curNote      = newTarget;
            _tgtNote      = newTarget;
            _totalSubBlks = T (0);
            _elapsed      = T (0);
        }
    }

    // Freeze glider at current note position.
    void noteOff () {
        _noteActive   = false;
        _tgtNote      = _curNote;
        _totalSubBlks = T (0);
        _elapsed      = T (0);
    }

    // Called when shift params change. Updates target immediately; glide continues.
    void updateShifts (int octShift, int semiShift, int centShift) {
        _tgtNote = _computeTarget (_lastMidiNote, octShift, semiShift, centShift);
        if (_totalSubBlks <= T (0))
            _curNote = _tgtNote;
    }

    // Called when glide time changes mid-glide. Adjusts remaining time proportionally.
    // New remaining = max(0, newTotal - elapsed). If zero, snaps to target.
    void updateGlideMs (T glideMs, T osSampleRate, T smallBlkSz) {
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

    // CC offset in note units, applied to the output (not the target).
    void setCCOffset (T offset) { _ccOffset = offset; }

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
        return utils::noteToFreq (_curNote + _ccOffset);
    }

    T    getCurrentHz ()   const { return utils::noteToFreq (_curNote + _ccOffset); }
    T    getCurrentNote () const { return _curNote; }
    bool isNoteActive ()   const { return _noteActive; }

private:

    T    _curNote      = T (69);
    T    _tgtNote      = T (69);
    int  _lastMidiNote = 69;
    bool _noteActive   = false;
    T    _totalSubBlks = T (0);
    T    _elapsed      = T (0);
    T    _ccOffset     = T (0);

    static T _computeTarget (int midiNote, int octShift, int semiShift, int centShift) {
        return T (midiNote) + T (octShift) * T (12) + T (semiShift) + T (centShift) / T (100);
    }
};


} // namespace hvoya
