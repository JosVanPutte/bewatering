
#include <stdint.h>

/**
 * returns the voltage from an ADC value
 */
double ReadADC(uint16_t reading);

/**
 * we use the average of 4 measurements of the voltage
 **/

double ReadAverage(uint16_t* analogValues, int count, uint16_t readin);