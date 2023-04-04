#include <Wifi.h>
#include <time.h>
#include "Sunrise.h"

#define HOUR_SECS 3600 // one hour in seconds
#define CET HOUR_SECS  // CET is one hour later than UTC
#define MARCH 2
#define NOVEMBER 10
#define SUNDAY 7

bool synced;

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
 * sync time with NTP
 */
void syncTime() {
  struct timeval tm;

  configTime(0, CET, "nl.pool.ntp.org");
  gettimeofday(&tm, NULL);
  settimeofday(&tm, NULL);
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

bool getAndCheckTime(struct tm& info) {
  bool ok = getLocalTime(&info);
  if (!synced && (WiFi.status() == WL_CONNECTED)) {
   // sync time
    syncTime();
    ok = getLocalTime(&info);
    Serial.printf("after sync: year %d %sok\n", info.tm_year, ok ? "" : "NOT ");
    synced = ok;
  }
  return ok;
}

unsigned long secondsToDisplay() {
  struct tm timeinfo;
  if (getAndCheckTime(timeinfo)) {
    bool summer = isSummerTime(timeinfo);
    int secsToSunUp = secondsToSunrise(timeinfo, summer);
    return secsToSunUp;
  }
  return 0;
}
/**
 * make a readable time string
 */
String timePeriodStringToSunup() {
  unsigned long units = secondsToDisplay();
  int s = units % 60; 
  units = units / 60; // mins
  int m = units % 60; 
  units = units / 60; // hours
  int h = units % 24; 
  return zeroPad(h) + ":" + zeroPad(m) + ":" + zeroPad(s);
}

/**
 * get the time in readable format
 */
String getTimeStr() {
  char buf[20];
  char *timestr = buf;
  struct tm timeinfo;
  getAndCheckTime(timeinfo);
  if (isSummerTime(timeinfo)) {
    ++timeinfo.tm_hour;
  }
  strftime(timestr, 20, "%D %R", &timeinfo);
  return String(timestr);
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
      Serial.printf("it is dark (%d h) so sleep for %d hours %d min until sunrise\n", hour, secsToSunUp / 3600, (secsToSunUp % 3600)/60);
      return secsToSunUp;   
    }
  }
  return 30;
}
