#include "scale.h"

bool getTareAsyncStatus() {
  return scaleTareSuccess;
}

#if (SCALE_SENSOR_ENABLE)
HX711_ADC LoadCell(SCALE_SENSOR_DATA_PIN, SCALE_SENSOR_CLK_PIN); 

/********************************************************
   helper functions
******************************************************/
void tareAsync() {
  LoadCell.tareNoDelay();
  scaleTareSuccess = false;
  currentWeight = 0;  //reset to 0 when waiting for tare()
}

void scalePowerDown() {
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
  if (!scaleTareSuccess && LoadCell.getTareStatus()) {
        scaleTareSuccess = true;
  }

  // get (smoothed) value from the dataset
  if (LoadCell.updateAsync()) {
    //only getData() when tare has completed
    if (scaleTareSuccess) currentWeight = LoadCell.getData();
    //DEBUG_print("* currentWeight: %0.2f (index=%d, SamplesInUse=%d, getSignalTimeoutFlag=%d)\n", currentWeight, LoadCell.getReadIndex(), LoadCell.getSamplesInUse(), LoadCell.getSignalTimeoutFlag());
  }

  unsigned long currentMillisScale = millis();
  if (currentMillisScale - previousMillisScale >= intervalWeightMessage) {
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
    if (LoadCell.getTareTimeoutFlag()) {
      ERROR_print("HX711 cannot be initialized (#%u)\n", i);
    }
    else {
      DEBUG_print("HX711 initialized (#%u)\n", i);
      LoadCell.setCalFactor(CALIBRATIONVALUE); // set calibration factor (float)
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