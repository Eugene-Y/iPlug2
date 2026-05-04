//
//  logger.cpp
//
//  Created by Eugene Yakshin on 09.11.2021.
//

#include <plog/Initializers/RollingFileInitializer.h>
#include "logger.hpp"


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
            _consoleAppender.disable();
            _logger->removeStatefulAppender (&_consoleAppender);
        }
    }

}
