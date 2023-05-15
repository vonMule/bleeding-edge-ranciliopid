#ifndef _scale_H
#define _scale_H

#include "userConfig.h"
#include "rancilio-pid.h"
#include "Arduino.h"
#include "display.h"
#include "Enums.h"

#if (SCALE_SENSOR_ENABLE)
#include "scaleConfigOverwrite.h"
#include <HX711_ADC.h>
extern HX711_ADC LoadCell;
#endif

bool scaleTareSuccess = false;
bool scaleRunning = false;

float currentWeight = 0.0;  // gram
float flowRate = 0.0;       // gram/second
float flowRateFactor = 0.5;  //moving average factor
unsigned long flowRateSampleTime = 0;
unsigned long flowRateEndTime = 0;

void initScale();
void tareAsync();
bool getTareAsyncStatus();  //returns true if tareAsync() has completed. else false
void updateWeight();
void scalePowerDown();
void scalePowerUp();

extern float* activeScaleSensorWeightSetPoint;
extern float scaleSensorWeightOffset;
extern void displaymessage(State, char*, char*);
#endif