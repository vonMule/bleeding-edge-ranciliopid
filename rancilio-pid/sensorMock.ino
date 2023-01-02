/***********************************
 *  Seonsor Mock for testing
 ***********************************/
#include "sensorMock.h"

float temperature_simulate_steam(float *minValue) {
  unsigned long now = millis();
  // if ( now <= 20000 ) return 102;
  // if ( now <= 26000 ) return 99;
  // if ( now <= 33000 ) return 96;
  // if (now <= 45000) return setPoint;
  if (now <= 20000) return 114;
  if (now <= 26000) return 117;
  if (now <= 29000) return 120;
  if (now <= 32000) return 116;
  if (now <= 35000) return 113;
  if (now <= 37000) return 109;
  if (now <= 39000) return 105;
  if (now <= 40000) return 101;
  if (now <= 43000) return 97;
  return *minValue;
}

float temperature_simulate_normal(float *maxValue) {
  unsigned long now = millis();
  if (now <= 12000) return 82;
  if (now <= 15000) return 85;
  if (now <= 19000) return 88;
  if (now <= 25000) return 91;
  if (now <= 28000) return 92;
  return *maxValue;
}