#pragma once

#include <string>


namespace hvoya {

		struct TimeInfo {
			size_t samplePos = 0;
			
			uint_fast8_t num = 4;
			uint_fast8_t den = 4;
			
			uint_fast32_t measure  = 0;
			uint_fast16_t beat	   = 0;
			uint_fast16_t fraction = 0;
			
			uint_fast32_t hours   = 0;
			uint_fast8_t  minutes = 0;
			uint_fast8_t  seconds = 0;
			uint_fast16_t milliseconds = 0;
			
			bool operator== (const TimeInfo& other) const {
				return samplePos == other.samplePos
						  && num == other.num
						  && den == other.den;
			}
			
			TimeInfo& operator= (const TimeInfo& other) {
				samplePos = other.samplePos;
				num 	    = other.num;
				den 	    = other.den;
				measure   = other.measure;
				beat	    = other.beat;
				fraction  = other.fraction;
				hours     = other.hours;
				minutes   = other.minutes;
				seconds   = other.seconds;
				milliseconds = other.milliseconds;
				return *this;
			}
		};
		
		
    std::string buildTimeString (const TimeInfo&);

}
