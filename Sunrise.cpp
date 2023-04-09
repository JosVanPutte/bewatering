/**
 * this code is adapted from Bens Hobby Corner to calculate the sunrise/sundown times
 * https://home.hccnet.nl/s.f.boukes/html-2/html-232.htm
 * so the system can go to sleep when there is no sun
 */ 
#include "Sunrise.h"
#include <math.h>
#include <HardwareSerial.h>

#define CET 1    // CET one hour ahead of UTC
#define LATITUDE 51.91626
#define LONGITUDE -4.36205

#define DEGREES (M_PI / 180)
/**
 * days in a month jan = 0
*/
int daysInMonth(int m, int y) {
  int d = 31;
  if (m==3||m==5||m==8||m==10) { d = 30;}
  // only feb is a challenge
  if (m != 1) { return d; }
  d = 28;
  if (!(y%4))   { d = 29;}
  if (!(y%100)) { d = 28;}
  if (!(y%400)) { d = 29;}
  return d;
}

/**
 * the day of the year
 */
int getDayOfYear(const struct tm& now) {
  int doy = now.tm_mday;
  int year = (now.tm_year + 1900);
  for (int m = 0; m < now.tm_mon; m++) {
    doy += daysInMonth(m, year);
  }
  return doy;
}

/**
 * angle to the sun
 */
double angleToSun(const struct tm& now) {
  double timeinyear = getDayOfYear(now) - 1.0 + (((now.tm_hour - 12.0) + (now.tm_min / 60.0))/24.0);
  return timeinyear*2.0*M_PI/365;
}
/**
 * equation time
 */
double eqTime(const struct tm& now) {
  double angle = angleToSun(now);
  return  229.18*(0.000075+0.001868*cos(angle) - 0.032077*sin(angle) - 0.014615*cos(2*angle) - 0.040849*sin(2*angle));
}

/**
 * declination angle to slope of earth
 */
double declination(const struct tm& now) {
  double angle = angleToSun(now);
  double declin = 0.006918-0.399912*cos(angle) + 0.070257*sin(angle) - 0.006758*cos(2*angle);
         declin = declin + 0.000907*sin(2*angle) - 0.002697*cos(3*angle) + 0.00148*sin(3*angle);
  return declin;
}

double horizontalAzimutCorrection(double declin) {
  double hars = cos(90.833 * DEGREES) / (cos(LATITUDE * DEGREES) * cos(declin));
         hars = hars - (tan(LATITUDE * DEGREES)*tan(declin));
         hars = acos(hars)/DEGREES;
  return hars;
}

/**
 * the time the sun rose today is almost the same as when it will rise tomorrow..
 */ 
int secondsToSunrise(const struct tm& now, bool summer) {
  int sunup = (int) (720.0 + 4* (LONGITUDE-horizontalAzimutCorrection(declination(now))) - eqTime(now));
  int hour = now.tm_hour;
  int sunupHour = sunup/60 + CET;
  int sunupMinute = sunup % 60;
  Serial.printf("it is %2d:%2d. Sunup at %02d:%02d (in %d hours)\n", hour + (summer ? 1 : 0), now.tm_min, sunupHour + (summer ? 1 : 0), sunupMinute, sunupHour + 24 - hour);
  return ((sunupHour + 24 - hour) * 60 + (sunupMinute - now.tm_min)) * 60;
}
/**
 * seconds to sunset
 */
int secondsToSunset(const struct tm& now, bool summer) {
  int sundown = (int) (720.0 + 4* (LONGITUDE+horizontalAzimutCorrection(declination(now))) - eqTime(now));
  int hour = now.tm_hour;
  int sunsetHour = sundown/60 + CET;
  int sunsetMinute = sundown % 60;
  Serial.printf("it is %2d:%2d. Sundown at %2d:%2d (in %d hours)\n", hour + (summer ? 1 : 0), now.tm_min, sunsetHour + (summer ? 1 : 0), sunsetMinute, sunsetHour - hour);
  return ((sunsetHour - hour) * 60 + (sunsetMinute - now.tm_min)) * 60;
}
/**
 * is it bedtime ?
 */
bool isSunDown(const struct tm& now, bool summer) {
  int secondsToDark = secondsToSunset(now, summer);
  return secondsToDark <= 0;
}
