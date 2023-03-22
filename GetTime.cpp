#include <Wifi.h>
#include <time.h>

#define CET 3600 // CET is one hour later than UTC

bool synced;

void getAndCheckTime(struct tm& info) {
   if (!getLocalTime(&info)) {
    Serial.println("Failed to get time");
  }
}
/**
 * get the time in readable format
 */
String getTimeStr() {
  if (!synced) {
    configTime(0, CET, "nl.pool.ntp.org");
    synced = true;
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
  getAndCheckTime(timeinfo);
  return (timeinfo.tm_hour <= 5);
}
