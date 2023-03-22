
#include "ReadADC.h"
#include <Math.h>

// ReadADC is used to improve the linearity of the ESP32 ADC see: https://github.com/G6EJD/ESP32-ADC-Accuracy-Improvement-function
double ReadADC(uint16_t reading) {
  if (reading >= 1 && reading <= 4095) {
    double polyVal = -0.000000000000016 * pow(reading,4) + 0.000000000118171 * pow(reading,3)- 0.000000301211691 * pow(reading,2)+ 0.001109019271794 * reading + 0.034143524634089;
    if (polyVal > 0.0) {
      return polyVal;
    }
  }
  return 0.0;
}

//*****************************************************************************
double ReadAverage(uint16_t* analogValues, int count, uint16_t readin) {
  double average = ReadADC(readin);
  int last = count - 1;
  int num  = count + 1;
  for (int i = 0; i < last; i++) {
    average += ReadADC(analogValues[i]);
    analogValues[i] = analogValues[i+1]; // i + 1 max is last
  }
  average += ReadADC(analogValues[last]);
  analogValues[last] = readin;
  return average / num;
}