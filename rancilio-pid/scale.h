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

float scaleSensorWeightSetpoint = SCALE_SENSOR_WEIGHT_SETPOINT;
float calibrationValue = CALIBRATIONVALUE;
//float weightPreBrew = 0;  // value of scale before wrew started
float weightBrew = 0;  // weight value of brew
float currentWeight = 0.0;
float scaleDelayValue = 2.5;  //value in gramm that takes still flows onto the scale after brew is stopped
bool scaleFailure = false;
const unsigned long intervalWeightMessage = 10000;   // weight scale
unsigned long previousMillisScale;  // initialisation at the end of init()

volatile boolean newDataReady = 0;

void initScale();
void updateWeight();

#endif