#include "host_time_info.hpp"


namespace hvoya {
		
		std::string buildTimeString (const TimeInfo& i) {
			std::string s;
			using std::to_string;
			s += to_string (i.beat) + " ";
			if (i.hours) s += to_string (i.hours) + "h:";
			s += to_string (i.minutes) + ":" + to_string (i.seconds);
			s += "." + to_string (i.milliseconds);
			return s;
		}
	
}
