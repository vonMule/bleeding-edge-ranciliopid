#ifndef TEMPERATURESENSOR_H
#define TEMPERATURESENSOR_H

#include "rancilio-enums.h"

class TemperatureSensor {
   
public:
  TemperatureSensor(int recovery);
  
  void init();
  char* getName();
  int getHistorySize();
  bool isMalfunction();
  float read();
  float readWithDelay();
  SensorStatus checkSensor(State activeState, float activeSetPoint, float secondlatestTemperature);
  void refresh(float* currentTemperature, State activeState, float activeSetPoint, float* secondlatestTemperature);
  void setPreviousTimerRefresh(unsigned long previousTimerRefreshTemp);
  float getCurrentTemperature();
  float getLatestTemperature();
  float pastTemperatureChange(int lookback);
  float getAverageTemperature(int lookback, int offsetReading);
  float getTemperature(int lookback);
  void updateTemperatureHistory(float myInput);


private:
  float getAverageTemperature(int lookback);
  float pastTemperatureChange(int lookback, bool enable_avg);

  float m_latestTemperature;

#if (TEMPSENSOR == 3)
  const unsigned long m_refreshInterval = 200; // How often to read the temperature sensor (must be >=180ms)
  const char* m_name = "MAX6675";
#elif (TEMPSENSOR == 9)
  const char* m_name = "MOCK";
  const unsigned long m_refreshInterval = 100; // How often to read the temperature sensor
#else // TSIC306 default sensor
  const unsigned long m_refreshInterval = 100; // How often to read the temperature sensor
  const char* m_name = "TSIC306";
#endif

  unsigned long m_previousTimerRefresh;

  int m_recovery;
  int m_error;
  int m_errorFarOff;
  const int m_maxErrorCounter = 10 * 10; // define maximum number of consecutive polls (of m_refreshInterval) to have errors
  bool malfunction;

  int m_readIndex; // the index of the current reading

  static const int numberOfReadings = 75 * 10; // number of values per Array
  float m_readingsTemp[numberOfReadings]; // the readings from Temp
  float m_readingsTime[numberOfReadings]; // the readings from time

};

#endif
