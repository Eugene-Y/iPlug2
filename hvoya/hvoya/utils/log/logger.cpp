//
//  logger.cpp
//
//  Created by Eugene Yakshin on 09.11.2021.
//

#include <chrono>
#include <thread>

#include <plog/Initializers/RollingFileInitializer.h>
#include "logger.hpp"


#ifndef PLOG_DISABLE_LOGGING

namespace hvoya {

    StatefulLogger::StatefulLogger (fIdProvider p1, fIdProvider p2) :
        _formatter (p1, p2),
        _consoleAppender (_formatter)
        {
            _logger = plog::get();
            if (_logger) {
                _logger->addAppender (&_consoleAppender);
            } else {
                _logger = &plog::init (plog::debug, &_consoleAppender);
            }
        }


    StatefulLogger::~StatefulLogger() {
        if (_logger) {
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
            _logger->removeStatefulAppender (&_consoleAppender);
        }
    }

}

#endif
