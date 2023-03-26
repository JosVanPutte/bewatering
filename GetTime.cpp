#include <Wifi.h>
#include <time.h>

#define CET 3600 // CET is one hour later than UTC

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
 * sync time with NTP
 */
void syncTime() {
  struct timeval tm;

  gettimeofday(&tm, NULL);
  Serial.printf("before sync: seconds %d", tm.tv_sec);
  configTime(0, CET, "nl.pool.ntp.org");
  gettimeofday(&tm, NULL);
  Serial.printf("after sync: seconds %d", tm.tv_sec);
  settimeofday(&tm, NULL);
}

bool getAndCheckTime(struct tm& info) {
   bool ok = getLocalTime(&info);
   // if time not synced failed, the year is 1970...
  Serial.printf("%sok. year %d\n", ok ? "" : "NOT ", info.tm_year);
  ok = (info.tm_year > 0);
  if (!ok) {
    syncTime();
    getLocalTime(&info);
    Serial.printf("after sync: year %d\n", info.tm_year);
    ok = (info.tm_year > 0);
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
 * check if it is night
 */
bool atNight() {
  struct tm timeinfo;
  bool ok = getAndCheckTime(timeinfo);
  return ok && (timeinfo.tm_hour <= 5 || timeinfo.tm_hour >= 22);
}
