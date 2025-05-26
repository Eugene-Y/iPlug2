#pragma once

#include <iomanip>
#include <functional>

#include <plog/Record.h>
#include <plog/Util.h>

#include "../../config.h"
#include "../../plug_build_info.hpp"


namespace hvoya {
    
    typedef std::function <size_t()> fIdProvider;    

    class StatefulFormatter {

        fIdProvider _instanceIdProvider;
        fIdProvider _bufIdProvider;

        public:
    
        StatefulFormatter (fIdProvider instidp = [](){ return 0; },
                           fIdProvider bidp    = [](){ return 0; })
          : _instanceIdProvider (instidp),
            _bufIdProvider (bidp) {}

        static plog::util::nstring header() { return plog::util::nstring(); }


        static plog::util::nstring format (const plog::Record& record) {
            plog::util::nostringstream ss;
            ss << plog::severityToString (plog::Severity::warning) << PLOG_NSTR ("  ") <<  PLUG_NAME 
                << PLOG_NSTR ("you are using it the wrong way");
            return ss.str();
        }


        plog::util::nstring statefulFormat (const plog::Record& record) const {
            tm t;
            plog::util::localtime_s (&t, &record.getTime().time);
            const auto setfill = [](auto&& s) { return std::setfill (PLOG_NSTR (s)); };
            using std::setw;

            plog::util::nostringstream ss;
            ss << setfill ('0') << setw (2) << t.tm_hour << PLOG_NSTR (":")
                << setfill ('0') << setw (2) << t.tm_min << PLOG_NSTR (":")
                << setfill ('0') << setw (2) << t.tm_sec << PLOG_NSTR (".")
                << setfill ('0') << setw (3) << static_cast<int> (record.getTime().millitm) << PLOG_NSTR (" ");

            ss << " | " << setfill (' ') << setw (6) << _bufIdProvider() << " | ";

            ss << setfill (' ') << setw (5) << std::left
               << PLUG_NAME << PLOG_NSTR ("_") << PLUG_PRODUCT <<  PLOG_NSTR ("_")
               << _instanceIdProvider() << PLOG_NSTR (' ');

            ss << setfill (' ') << setw (5) << std::left << plog::severityToString(record.getSeverity()) << PLOG_NSTR ("  ");
			
            //ss << PLOG_NSTR ("[") << record.getFunc() << PLOG_NSTR ("@") << record.getLine() << PLOG_NSTR ("] ");
			
            ss << record.getMessage() << PLOG_NSTR ("\n");

            return ss.str();
        }
    };
    
}
