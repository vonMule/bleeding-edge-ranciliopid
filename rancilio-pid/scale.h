#ifndef _scale_H
#define _scale_H

#include "userConfig.h"
#include "rancilio-pid.h"
#include "Arduino.h"

#if (SCALE_SENSOR_ENABLE)
#include "scaleConfigOverwrite.h"
#include <HX711_ADC.h>
extern HX711_ADC LoadCell;
#endif

bool scaleTareSuccess = false;
bool scaleRunning = false;

float weightBrew = 0;  // weight value of brew
float currentWeight = 0.0;
const unsigned long intervalWeightMessage = 10000;
unsigned long previousMillisScale;

void initScale();
void tareAsync();
bool getTareAsyncStatus();  //returns true if tareAsync() has completed. else false
void updateWeight();
void scalePowerDown();
void scalePowerUp();
#endif