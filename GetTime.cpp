#include <Wifi.h>
#include <time.h>

#define CET 3600 // CET is one hour later than UTC

bool synced;
bool ok;

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

void getAndCheckTime(struct tm& info) {
   if (!getLocalTime(&info)) {
    ok = false;
    Serial.println("Failed to get time");
  } else {
    // if time sync failed, the year is 1970...
    ok = (info.tm_year > 2000);
  }
}
/**
 * get the time in readable format
 */
String getTimeStr() {
  if (!synced) {
    configTime(0, CET, "nl.pool.ntp.org");
    synced = ok;
  }
  struct tm timeinfo;
  getAndCheckTime(timeinfo);
  return String(asctime(&timeinfo));
}

/**
 * check if it is night
 */
bool atNight() {
  struct tm timeinfo;
  if (!synced) {
    getTimeStr();
  }
  getAndCheckTime(timeinfo);
  return ok && (timeinfo.tm_hour <= 5 || timeinfo.tm_hour >= 22);
}
