/********************************************************
 * Perfect Coffee PID
 * https://github.com/medlor/bleeding-edge-ranciliopid
 *****************************************************/
#include "scale.h"

bool getTareAsyncStatus() {
  return scaleTareSuccess;
}

#if (SCALE_SENSOR_ENABLE)
HX711_ADC LoadCell(SCALE_SENSOR_DATA_PIN, SCALE_SENSOR_CLK_PIN); 

/********************************************************
   helper functions
******************************************************/

void scaleCalibration() {
  static unsigned long lastCalibration = 10000;
  if ( millis() >= lastCalibration + 2500) {
    lastCalibration = millis();

    if (!scaleRunning) {
      scalePowerUp();
      tareAsync();
    } else if (getTareAsyncStatus()) {
      float newCalibrationValue = LoadCell.getNewCalibration((float)SCALE_SENSOR_CALIBRATION_WEIGHT);
      DEBUG_print("Scale is tared to zero: You can now put your weight of %0.2fg on the scale: currentWeight=%0.2f. Calculated SCALE_SENSOR_CALIBRATION_FACTOR=%0.2f\n", (float)SCALE_SENSOR_CALIBRATION_WEIGHT, currentWeight, newCalibrationValue);   
      if ((currentWeight > 10) && (newCalibrationValue > 50) && (abs(LoadCell.getCalFactor() - newCalibrationValue) >= 5)) { 
        DEBUG_print("Scale calibration setting saved.\n");
        LoadCell.setNewCalibration((float)SCALE_SENSOR_CALIBRATION_WEIGHT);
        //TODO: save to eeprom and create "auto-calibration" ACTION
      }
    }
  }
}

void tareAsync() {
  LoadCell.tareNoDelay();
  scaleTareSuccess = false;
  currentWeight = 0;  //reset to 0 when waiting for tare()
  flowRate = 0.0;
  flowRateEndTime = millis() + 30000;
}

void scalePowerDown() {
  if (!scaleRunning) return;
  scaleRunning = false;
  LoadCell.powerDown();
}

void scalePowerUp() {
  scaleRunning = true;
  currentWeight = 0;
  LoadCell.powerUp();
}

void updateWeight() {
  if (!scaleRunning) return;
  //check if tareAsync() has triggered a tare() and read status of tare()
  //scaleTareSuccess = true; //XXX3 remove
  if (!scaleTareSuccess && LoadCell.getTareStatus()) {
        scaleTareSuccess = true;
        flowRateSampleTime = millis() - 100;
  }

  // get (smoothed) value from the dataset
  if (LoadCell.updateAsync()) {
    //only getData() when tare has completed
    if (scaleTareSuccess) {

      float previousWeight = currentWeight;
      currentWeight = LoadCell.getData();
      float diffWeight = currentWeight - previousWeight;
      float remainingWeight = *activeScaleSensorWeightSetPoint - scaleSensorWeightOffset - currentWeight ;

      unsigned long prevFlowRateSampleTime = flowRateSampleTime;
      flowRateSampleTime = millis();
      unsigned long diffFlowRateSampleTime = flowRateSampleTime - prevFlowRateSampleTime;
      if (diffFlowRateSampleTime > 110 || diffFlowRateSampleTime <= 11) {  //regular refresh on 10SPS every 90ms
        ERROR_print("flowRateSampleTime anomaly: %lums\n", diffFlowRateSampleTime);
        return;  //ignore sample //XXX3 readd
      }
      //diffFlowRateSampleTime = 100.0; //XXX3 remove

      float currentFlowRate = ( diffWeight * 1000.0 / diffFlowRateSampleTime) ;  //inaccuracy up tp 0.15g/s
      flowRate = (flowRate * (1-flowRateFactor) ) + (currentFlowRate * flowRateFactor); // smoothed gram/s
      if (flowRate <= 0.1) flowRate = 0.1;  //just be sure to never have negative flowRate
      
      if (abs(flowRate) <= 0.1) {
        //DEBUG_print("flowRate low: %0.2fg/s\n", flowRate);
        flowRateEndTime = millis() + 30000;  //during pre-infusion or after brew() we need special handling
      } else if (remainingWeight > 0) {
        int offsetTime = 0;
        flowRateEndTime = millis() + (unsigned long)((remainingWeight / flowRate) * 1000) - offsetTime; //in how many ms is the weight reached
      } else {
        flowRateEndTime = millis();  //weight already reached
      }

      //DEBUG_print("updateWeight(%lums): weight=%0.3fg/Diff=%0.2fg/Offset=%0.2f currentFlowRate=%0.2fg/s flowRate=%0.2fg/s flowRateEndTime=%lums\n", 
      //  diffFlowRateSampleTime, currentWeight, diffWeight, scaleSensorWeightOffset, currentFlowRate, flowRate, (flowRateEndTime - millis()));
    }
  }

  unsigned long currentMillisScale = millis();
  if (currentMillisScale - previousMillisScale >= intervalWeightMessage) {  //XXX1 remove this whole debug block
    previousMillisScale = currentMillisScale;
    DEBUG_print("currentWeight: %0.2f (index=%d, SamplesInUse=%d, getSignalTimeoutFlag=%d)\n", currentWeight, LoadCell.getReadIndex(), LoadCell.getSamplesInUse(), LoadCell.getSignalTimeoutFlag());
  }
}

/********************************************************
   Initialize scale
******************************************************/
static void attachISR_ESP32_SCALE(void *arg){					//attach ISR in freeRTOS because arduino can't attachInterrupt() inside of template class
  //DEBUG_print("attachISR_ESP32_2()\n");
  gpio_config_t gpioConfig;
  gpioConfig.pin_bit_mask = ((uint64_t)(((uint64_t)1)<<SCALE_SENSOR_DATA_PIN));
  gpioConfig.mode         = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en   = GPIO_PULLUP_ENABLE ; //convertPortModeToDefine(_portMode) == INPUT_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE; //convertPortModeToDefine(_portMode) == INPUT_PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
  gpioConfig.intr_type    = GPIO_INTR_NEGEDGE;
  ESP_ERROR_CHECK(gpio_config(& gpioConfig));
  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)SCALE_SENSOR_DATA_PIN, dataReadyISR, NULL);
  vTaskDelete(NULL);
}

void initScale() {
  long stabilizingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  for (int i=0; i<3;i++) {
    LoadCell.begin();
    LoadCell.start(stabilizingtime, _tare);
    //DEBUG_print("currentWeight: %0.2f (index=%d, getTareTimeoutFlag=%d, getSignalTimeoutFlag=%d)\n", currentWeight, LoadCell.getReadIndex(), LoadCell.getTareTimeoutFlag(), LoadCell.getSignalTimeoutFlag());
    if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
      ERROR_print("HX711 cannot be initialized (#%u)\n", i);
      displaymessage(0, (char*)"Scale sensor defect", (char*)"");
    }
    else {
      DEBUG_print("HX711 initialized (#%u)\n", i);
      LoadCell.setCalFactor(SCALE_SENSOR_CALIBRATION_FACTOR); // set calibration factor (float)
      #ifdef ESP32
      //CPU pinning does not have an effect (TSIC). Why?
      xTaskCreatePinnedToCore(attachISR_ESP32_SCALE,"attachISR_ESP32_SCALE",2000,NULL,1,NULL,1); //freeRTOS
      #else
      attachInterrupt(digitalPinToInterrupt(SCALE_SENSOR_DATA_PIN), dataReadyISR, FALLING);
      #endif
      break;
    }
    delay(200);
  }
}

void IRAM_ATTR dataReadyISR(void * arg) {
  LoadCell.dataWaitingAsync();
}

#endif