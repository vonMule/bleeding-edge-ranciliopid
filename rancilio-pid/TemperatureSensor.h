#ifndef TEMPERATURESENSOR_H
#define TEMPERATURESENSOR_H

// : 0 := OK, 1 := Hardware issue, 2:= Software issue / outlier detected, 3: temperature jump
enum SensorStatus {
    Ok = 0,
    HardwareIssue = 1,
    SoftwareIssue = 2,
    TemperatureJump = 3
};

class TemperatureSensor {
   
public:
  TemperatureSensor(int recovery);
  
  void init();
  float read();
  float readWithDelay();
  SensorStatus checkSensor(int activeState, float* activeSetPoint, float* secondlatestTemperature);
  float refresh(float previousValue, int activeState, float* activeSetPoint, float* secondlatestTemperature);
  void setPreviousTimerRefresh(unsigned long previousTimerRefreshTemp);
  float getCurrentTemperature();
  float getLatestTemperature();
  float pastTemperatureChange(int lookback);
  float getAverageTemperature(int lookback, int offsetReading);
  float getTemperature(int lookback);
  void updateTemperatureHistory(float myInput);

  const char* name;
  bool malfunction;

  static const int numberOfReadings = 75 * 10; // number of values per Array

private:
  float getAverageTemperature(int lookback);
  float pastTemperatureChange(int lookback, bool enable_avg);

  float m_latestTemperature;

  int m_refreshInterval;
  unsigned long m_previousTimerRefresh;

  int m_recovery;

  int m_error;
  int m_errorFarOff;
  int m_maxErrorCounter; // define maximum number of consecutive polls (of m_refreshInterval) to have errors

  int m_readIndex; // the index of the current reading
  float m_readingsTemp[numberOfReadings]; // the readings from Temp
  float m_readingsTime[numberOfReadings]; // the readings from time

};

#endif
