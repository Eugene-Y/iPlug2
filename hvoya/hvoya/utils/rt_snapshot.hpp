#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace hvoya {

    // Single-producer / single-consumer lock-free handoff of a whole snapshot value.
    //
    // One thread (the producer — typically the UI/message thread) calls publish() to
    // hand a fresh copy of T across; another thread (the consumer — typically the audio
    // thread) calls load() once per block to pull the latest. Neither thread blocks,
    // allocates, or takes a lock, so it is safe to call load() from the realtime path.
    //
    // Implemented as a seqlock: the producer brackets the body write with an odd→even
    // sequence counter; the consumer reads the body between two even reads of the
    // counter and accepts the copy only if the counter was stable across it. On the
    // rare contended read (producer mid-publish) the consumer gives up after a bounded
    // number of attempts and leaves the caller's destination untouched — so the caller
    // keeps its last good copy and the realtime path never spins unboundedly.
    //
    // T must be trivially copyable (the body is moved with memcpy). The whole T is
    // transferred atomically-as-a-unit, so the consumer never sees a half-updated
    // value the way it would reading the producer's live struct directly. The internal
    // swap can be replaced (e.g. with a triple buffer) behind this same API without
    // touching callers.
    template <class T>
    class RTSnapshot {
        static_assert (std::is_trivially_copyable_v<T>,
                       "RTSnapshot<T> transfers T by memcpy — T must be trivially copyable");

    public:
        RTSnapshot() = default;
        explicit RTSnapshot (const T& initial) { publish (initial); }

        // Producer thread. Publishes a new snapshot. Never blocks.
        void publish (const T& v) {
            const uint32_t s = _seq.load (std::memory_order_relaxed);
            _seq.store (s + 1, std::memory_order_relaxed);          // mark "writing" (odd)
            std::atomic_thread_fence (std::memory_order_release);
            std::memcpy (&_data, &v, sizeof (T));                   // body
            std::atomic_thread_fence (std::memory_order_release);
            _seq.store (s + 2, std::memory_order_relaxed);          // publish (even)
        }

        // Consumer thread. Tries to read the latest snapshot into `out`. Returns true on
        // a clean read; on contention leaves `out` untouched and returns false (the caller
        // keeps its previous copy). Never blocks, never allocates.
        bool load (T& out) const {
            for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
                const uint32_t s0 = _seq.load (std::memory_order_acquire);
                if (s0 & 1u) continue;                              // producer mid-publish
                T tmp;
                std::memcpy (&tmp, &_data, sizeof (T));             // body
                std::atomic_thread_fence (std::memory_order_acquire);
                const uint32_t s1 = _seq.load (std::memory_order_acquire);
                if (s0 == s1) { out = tmp; return true; }           // counter stable → consistent
            }
            return false;                                          // contended → caller keeps last good
        }

    private:
        static constexpr int kMaxAttempts = 8;

        // Counter on its own cache line so the producer's stores don't false-share the body.
        alignas (64) std::atomic<uint32_t> _seq { 0 };
        T _data {};   // guarded by _seq
    };

} // namespace hvoya
