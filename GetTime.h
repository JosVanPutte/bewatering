/**
 * make a string for a period of seconds
 */
String makeTimePeriodString(unsigned long seconds);

/**
 * get the time in readable format
 */
String getTimeStr();

/**
 * check if it is night
 * and sleep until 5 AM
 * returns seconds to sleep
 */
unsigned long sleepDuration();
