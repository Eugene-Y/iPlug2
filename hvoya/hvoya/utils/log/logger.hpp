//
//  logger.hpp
//
//  Created by Eugene Yakshin on 09.11.2021.
//

#pragma once

#include <plog/Log.h>
#include "log_formatter.hpp"
#include "log_appender.hpp"


namespace hvoya {

#ifndef PLOG_DISABLE_LOGGING

    // TODO add indent control
    class StatefulLogger {
        public:
            StatefulLogger (fIdProvider bufIdp = [](){ return 0; },
                            fIdProvider instanceIdp = [](){ return 0; });
            virtual ~StatefulLogger();

        private:
            StatefulFormatter _formatter;
            ConsoleAppender <StatefulFormatter> _consoleAppender;
            plog::Logger <PLOG_DEFAULT_INSTANCE_ID>* _logger;
    };

#else

    class StatefulLogger {
        public:
            StatefulLogger (fIdProvider = [](){ return 0; },
                            fIdProvider = [](){ return 0; }) {}
    };

#endif

}
