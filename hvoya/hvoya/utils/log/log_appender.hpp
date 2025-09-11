#pragma once

#include <plog/Appenders/ConsoleAppender.h>

namespace hvoya {

    template <typename Formatter>
    class ConsoleAppender : public plog::ConsoleAppender <Formatter> {

        typedef plog::ConsoleAppender <Formatter> base_t;

        private:
            const Formatter& _formatter;
       
        public:
        
            ConsoleAppender (const Formatter& formatter) : 
                plog::ConsoleAppender <Formatter> (plog::OutputStream::streamStdOut), 
                _formatter (formatter) {}
            
            virtual void write (const plog::Record& r) override {
                plog::util::nstring str = _formatter.statefulFormat (r);
                plog::util::MutexLock lock (base_t::m_mutex);
                base_t::writestr (str);
            }
    };
    
}
