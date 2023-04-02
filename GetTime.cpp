#include <Wifi.h>
#include <time.h>
#include "Sunrise.h"

#define HOUR_SECS 3600 // one hour in seconds
#define CET HOUR_SECS  // CET is one hour later than UTC
#define MARCH 2
#define NOVEMBER 10
#define SUNDAY 7

/**
 * zeropad time values
 */
String zeroPad(int n) {
  String retval = String(n);
  if (n >= 10) {
    return retval;
  }
  return String("0") + retval;
}

/**
 * make a readable time string
 */
String makeTimePeriodString(unsigned long seconds) {
  unsigned long units = seconds;
  int s = units % 60; 
  units = units / 60; // mins
  int m = units % 60; 
  units = units / 60; // hours
  int h = units % 24; 
  units = units / 24; // days
  return String(units) + " d " + zeroPad(h) + ":" + zeroPad(m) + ":" + zeroPad(s);
}

/**
 * summertime is toggled at the last sunday in march and november
 */
bool isSummerTime(const struct tm& info) {
  int month = info.tm_mon;
  int day = info.tm_mday;
  int weekday = info.tm_wday;
  bool afterToggle = (day + (SUNDAY - (weekday % SUNDAY))) >= 31;
  bool dst = (month >= MARCH) && (month < NOVEMBER);
  if (MARCH != month  && NOVEMBER != month) {
    Serial.printf("month %d ==> %s\n", month, dst ? "summer" : "winter");
    return dst;
  }
  // now check if it is (past) the last sunday of the month
  dst = (afterToggle && MARCH == month) || (!afterToggle && NOVEMBER == month);  
  Serial.printf("%s day %d weekday %d (%s last sunday) ==> %s\n", MARCH == month ? "march" : "november", day, weekday, afterToggle ? "after" : "before", dst ? "summer" : "winter");
  return dst;
}


/**
 * sync time with NTP
 */
void syncTime() {
  struct timeval tm;

  configTime(0, CET, "nl.pool.ntp.org");
  gettimeofday(&tm, NULL);
  settimeofday(&tm, NULL);
}

bool getAndCheckTime(struct tm& info) {
  bool ok = getLocalTime(&info);
  if (!ok) {
   // if time not synced failed, the year is (19)70...
    Serial.printf("%sok. year %d\n", ok ? "" : "NOT ", info.tm_year);
    syncTime();
    ok = getLocalTime(&info);
    Serial.printf("after sync: year %d %sok\n", info.tm_year, ok ? "" : "NOT ");
  }
  return ok;
}
/**
 * get the time in readable format
 */
String getTimeStr() {
  struct tm timeinfo;
  getAndCheckTime(timeinfo);
  return String(asctime(&timeinfo));
}

/**
 * check if it is night and return a good nights sleep
 * in seconds
 */
unsigned long sleepDuration() {
  struct tm timeinfo;
  bool ok = getAndCheckTime(timeinfo);
  int hour = timeinfo.tm_hour;
  if (ok) {
    bool summer = isSummerTime(timeinfo);
    bool dark = isSunDown(timeinfo, summer);
    int secsToSunUp = secondsToSunrise(timeinfo, summer);
    if (summer) { hour = hour + 1; }    
    if (dark) {
      // sleep until sunrise
      Serial.printf("it is dark (%d h) so sleep for %d hours %d min until sunrise\n", hour, secsToSunUp / 3600, (secsToSunUp % 3600) / 60);
      return secsToSunUp;   
    }
  }
  return 30;
}
