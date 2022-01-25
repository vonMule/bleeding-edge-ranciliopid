#if (BREWTIMER_MODE == 2) // SCALE Brewmode
#include <HX711_ADC.h>

float weightSetpoint = WEIGHTSETPOINT;
float calibrationValue = CALIBRATIONVALUE;
float weightPreBrew = 0;  // value of scale before wrew started
float weightBrew = 0;  // weight value of brew
float currentWeight = 0.0;
float scaleDelayValue = 2.5;  //value in gramm that takes still flows onto the scale after brew is stopped
bool scaleFailure = false;
const unsigned long intervalWeightMessage = 5000;   // weight scale
unsigned long previousMillisScale;  // initialisation at the end of init()
HX711_ADC LoadCell(HXDATPIN, HXCLKPIN);


/********************************************************
  getWeight
******************************************************/

void updateWeight() {
  //XXX1 check performance impact. optimize
  static boolean newDataReady = false;
  unsigned long currentMillisScale = millis();
  if (scaleFailure) {   // abort if scale is not working
    return;
  }

  // check for new data/start next conversion:
  if (LoadCell.update()) {
    newDataReady = true;
  }

  // get smoothed value from the dataset:
  if (newDataReady) {
    currentWeight = LoadCell.getData();
    weightBrew = currentWeight - weightPreBrew;
    newDataReady = false;
  }

  if (currentMillisScale - previousMillisScale >= intervalWeightMessage) {
    previousMillisScale = currentMillisScale;
    DEBUG_print("weightBrew: %0.2f\n", weightBrew);
  }
}

/********************************************************
   Initialize scale
******************************************************/
void initScale() {
  LoadCell.begin();
  long stabilizingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  DEBUG_print("INIT: Initializing scale ... ");
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    DEBUG_println("Timeout, check MCU>HX711 wiring and pin designations");
  }
  else {
    DEBUG_println("done");
  }
  LoadCell.setCalFactor(CALIBRATIONVALUE); // set calibration factor (float)
  LoadCell.setSamplesInUse(1);
}

#endif