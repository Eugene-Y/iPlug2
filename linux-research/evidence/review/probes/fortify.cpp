#include <cstdio>
#include <cstring>
#include <ctime>
// hvoya's CurrentTime() body, verbatim from IPlug/IPlugLogger.h
static const char* CurrentTime()
{
    //    TODO: replace with std::chrono based version
    time_t t = time(0);
    tm* pT = localtime(&t);
    
    char cStr[32];
    strftime(cStr, 32, "%Y%m%d %H:%M ", pT);
    
    int tz = 60 * pT->tm_hour + pT->tm_min;
    int yday = pT->tm_yday;
    pT = gmtime(&t);
    tz -= 60 * pT->tm_hour + pT->tm_min;
    yday -= pT->tm_yday;
    if (yday != 0)
    {
      if (yday > 1) yday = -1;
      else if (yday < -1) yday = 1;
      tz += 24 * 60 * yday;
    }
    int i = (int) strlen(cStr);
    cStr[i++] = tz >= 0 ? '+' : '-';
    if (tz < 0) tz = -tz;
    snprintf(&cStr[i], 32, "%02d%02d", tz / 60, tz % 60);
    
    static char sTimeStr[32];
    strcpy(sTimeStr, cStr);
    return sTimeStr;
  }
int main(){ puts(CurrentTime()); }
