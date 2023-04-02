#include <time.h>

/**
 * is the sun down ?
 */
bool isSunDown(const struct tm& now, bool summer);

/**
 * how long until it is up ?
 */
int secondsToSunrise(const struct tm& now, bool summer);