#pragma once

#include <atomic>
#include <plog/Appenders/ConsoleAppender.h>

namespace hvoya {

    template <typename Formatter>
    class ConsoleAppender : public plog::ConsoleAppender <Formatter> {

        typedef plog::ConsoleAppender <Formatter> base_t;

        const Formatter& _formatter;
        std::atomic<bool> _enabled { true };

    public:

        ConsoleAppender (const Formatter& formatter) :
            plog::ConsoleAppender <Formatter> (plog::OutputStream::streamStdOut),
            _formatter (formatter) {}

        // Call before removeStatefulAppender in ~StatefulLogger so that any
        // concurrent write() on the audio thread bails out before touching
        // _formatter (which is about to be destroyed).
        void disable() { _enabled.store (false, std::memory_order_release); }

        virtual void write (const plog::Record& r) override {
            if (!_enabled.load (std::memory_order_acquire)) return;
            plog::util::nstring str = _formatter.statefulFormat (r);
            plog::util::MutexLock lock (base_t::m_mutex);
            base_t::writestr (str);
        }
    };

}
