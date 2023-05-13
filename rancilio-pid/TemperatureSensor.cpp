#include "TemperatureSensor.h"
#include "userConfig.h"
#include "helper.h"


#include <RemoteDebug.h> //https://github.com/JoaoLopesF/RemoteDebug
extern RemoteDebug Debug;

#ifndef DEBUGMODE
#define DEBUG_print(fmt, ...)
#define DEBUG_println(a)
#define ERROR_print(fmt, ...)
#define ERROR_println(a)
#define DEBUGSTART(a)
#else
#define DEBUG_print(fmt, ...)                                                                                                                                                      \
  if (Debug.isActive(Debug.DEBUG)) Debug.printf("%0lu " fmt, millis() / 1000, ##__VA_ARGS__)
#define DEBUG_println(a)                                                                                                                                                           \
  if (Debug.isActive(Debug.DEBUG)) Debug.printf("%0lu %s\n", millis() / 1000, a)
#define ERROR_print(fmt, ...)                                                                                                                                                      \
  if (Debug.isActive(Debug.ERROR)) Debug.printf("%0lu " fmt, millis() / 1000, ##__VA_ARGS__)
#define ERROR_println(a)                                                                                                                                                           \
  if (Debug.isActive(Debug.ERROR)) Debug.printf("%0lu %s\n", millis() / 1000, a)
#define DEBUGSTART(a) Serial.begin(a);
#endif

#define STATE_COLDSTART 1 // TODO alex remove this
#define STATE_INNER_ZONE_DETECTED 3 // TODO alex remove this

#if (TEMPSENSOR == 3)
  #include <max6675.h>
  MAX6675 thermocouple(pinTemperatureCLK, pinTemperatureCS, pinTemperatureSO);
#elif (TEMPSENSOR == 9)
  #include <sensorMock.h>
#else   
  #include <ZACwire.h>
#if (!defined(ZACWIRE_VERSION) || (defined(ZACWIRE_VERSION) && ZACWIRE_VERSION < 200L))
  #error ERROR ZACwire library version must be >= 2.0.0
#endif
  ZACwire TSIC(pinTemperature, 306, true); 
#endif    

TemperatureSensor::TemperatureSensor(int recovery) {
  m_recovery = recovery;

  malfunction = false;
  m_error = 0;
  m_errorFarOff = 0;
  m_maxErrorCounter = 10*10;

  m_readIndex = 0; // the index of the current reading

#if (TEMPSENSOR == 3)
  m_refreshInterval = 200; // How often to read the temperature sensor (must be >=180ms)
  name = "MAX6675";
#elif (TEMPSENSOR == 9)
  name = "MOCK";
  m_refreshInterval = 100; // How often to read the temperature sensor
#else // TSIC306 default sensor
  m_refreshInterval = 100; // How often to read the temperature sensor
  name = "TSIC306";
#endif
}

void TemperatureSensor::init() {

  m_latestTemperature = 0;

  /********************************************************
  * moving average ini array
  ******************************************************/
  for (int thisReading = 0; thisReading < numberOfReadings; thisReading++) {
    m_readingsTemp[thisReading] = 0;
    m_readingsTime[thisReading] = 0;
  }

#if (TEMPSENSOR == 2)    
    if (TSIC.begin() != true) { ERROR_println("Temp sensor cannot be initialized"); }
    delay(120);
#endif    
}

float TemperatureSensor::readWithDelay() {
  delay(m_refreshInterval);
  return read();
}

float TemperatureSensor::read() {
#if (TEMPSENSOR == 3)
    //this sensor's reading flaps 0.5degrees on every read. Also calculate averages to mitigate PID issues.
    float past_average = getAverageTemperature(3,0);
    if (past_average == 0) {
      m_latestTemperature = thermocouple.readCelsius();
    } else {
      m_latestTemperature = (3*getAverageTemperature(3,0) + thermocouple.readCelsius()) / 4.0;
    }
#elif (TEMPSENSOR == 9)
    m_latestTemperature = temperature_simulate_normal(94.2f);
    //return temperature_simulate_normal(94.2f);
    //return temperature_simulate_steam(94.2f);
#else
    m_latestTemperature = TSIC.getTemp(250U);
#endif
    return m_latestTemperature;
}

/********************************************************
 * check sensor value. If there is an issue, increase error value. 
 * If error is equal to maxErrorCounter, then set sensorMalfunction.
 * m_latestTemperature(=latest read sample) is read one sample (100ms) after secondlatestTemperature.
 * Returns: 0 := OK, 1 := Hardware issue, 2:= Software issue / outlier detected
 *****************************************************/
SensorStatus TemperatureSensor::checkSensor(int activeState, float* activeSetPoint, float* secondlatestTemperature) {
  SensorStatus sensorStatus = HardwareIssue;
  if (malfunction) {
    if (m_recovery == 1 && m_latestTemperature >= 0 && m_latestTemperature <= 150) {
      malfunction = false;
      m_error = 0;
      m_errorFarOff = 0;
      sensorStatus = Ok;
      ERROR_print("temp sensor recovered\n");
    }
    return sensorStatus;
  }
  
  if (m_latestTemperature == 221) {
    m_error+=10;
    ERROR_print("temp sensor connection broken: consecErrors=%d, secondlatestTemperature=%0.2f, m_latestTemperature=%0.2f\n",
        m_error, secondlatestTemperature, m_latestTemperature);
  } else if (m_latestTemperature == 222) {
    m_error++;
    DEBUG_print("temp sensor read failed: consecErrors=%d, secondlatestTemperature=%0.2f, m_latestTemperature=%0.2f\n",
        m_error, secondlatestTemperature, m_latestTemperature);
  } else if (m_latestTemperature < 0 || m_latestTemperature > 150) {
    m_error++;
    DEBUG_print("temp sensor read unrealistic: consecErrors=%d, secondlatestTemperature=%0.2f, m_latestTemperature=%0.2f\n",
        m_error, secondlatestTemperature, m_latestTemperature); 
  } else if (fabs(m_latestTemperature - *secondlatestTemperature) > 5) {
    m_error++;
    m_errorFarOff++;
    
    //support corner-case if due to some hangup the temperature jumps >5 degree.
    if (m_errorFarOff >= (m_maxErrorCounter/2)) {
      sensorStatus = SoftwareIssue;
      ERROR_print("temp sensor read far off fixed: consecErrors=%d, secondlatestTemperature=%0.2f, m_latestTemperature=%0.2f\n",
        m_error, secondlatestTemperature, m_latestTemperature);
    } else {
      DEBUG_print("temp sensor read far off: consecErrors=%d, secondlatestTemperature=%0.2f, m_latestTemperature=%0.2f\n",
        m_error, secondlatestTemperature, m_latestTemperature);
    }
#ifdef DEV_ESP
  } else if ((activeState == STATE_INNER_ZONE_DETECTED || activeState == STATE_COLDSTART)  &&
     fabs(m_latestTemperature - secondlatestTemperature) >= 0.2 &&
     fabs(secondlatestTemperature - getTemperature(0)) >= 0.2 && 
     signnum(getTemperature(0) - secondlatestTemperature)*signnum(m_latestTemperature - secondlatestTemperature) > 0
     ) {
#else
  } else if (activeState == STATE_INNER_ZONE_DETECTED &&
     //fabs(secondlatestTemperature - setPoint) <= 5 &&
     fabs(m_latestTemperature - *activeSetPoint) <= 5 &&
     fabs(m_latestTemperature - *secondlatestTemperature) >= 0.2 &&
     fabs(*secondlatestTemperature - getTemperature(0)) >= 0.2 &&
     //fabs(m_latestTemperature - getTemperature(0)) <= 0.2 && //this check could be added also, but then return sensorStatus=1. 
     signnum(getTemperature(0) - *secondlatestTemperature)*signnum(m_latestTemperature - *secondlatestTemperature) > 0
     ) {
#endif
      m_error++;
      DEBUG_print("temp sensor inaccuracy: thirdlatestTemperature=%0.2f, secondlatestTemperature=%0.2f, m_latestTemperature=%0.2f\n",
        getTemperature(0), secondlatestTemperature, m_latestTemperature);
      sensorStatus = SoftwareIssue;
  } else {
    m_error = 0;
    m_errorFarOff = 0;
    sensorStatus = Ok;
  }
  if (m_error >= m_maxErrorCounter) {
    malfunction = true;
  }

  return sensorStatus;
}

void TemperatureSensor::setPreviousTimerRefresh(unsigned long previousTimerRefresh) {
    m_previousTimerRefresh = previousTimerRefresh;
}

float TemperatureSensor::refresh(float previousValue, int activeState, float* activeSetPoint, float* secondlatestTemperature) {
  unsigned long refreshTimeDiff = millis() - m_previousTimerRefresh;
  if (refreshTimeDiff >= m_refreshInterval) {
    if (refreshTimeDiff >= (m_refreshInterval *1.5)) {
      ERROR_print("refresh(): Delay=%lu ms (loop() hang?)\n", refreshTimeDiff - m_refreshInterval);
    }
    m_latestTemperature = read();
    SensorStatus sensorStatus = checkSensor(activeState, activeSetPoint, secondlatestTemperature);
    if (sensorStatus != Ok) {
      ERROR_print("temp sensorStatus: %d\n", (int)sensorStatus);
    }

    m_previousTimerRefresh = millis();
    if (sensorStatus == HardwareIssue) {  //hardware issue
      return previousValue;
    } else if (sensorStatus == SoftwareIssue || sensorStatus == TemperatureJump) {  //software issue: outlier detected(==2) or temperature jump (==3)
      updateTemperatureHistory(m_latestTemperature);  //use currentTemp as replacement
    } else {
      updateTemperatureHistory(*secondlatestTemperature);
    }

    *secondlatestTemperature = m_latestTemperature;
    return getAverageTemperature(5*10);
  }
  else {
    return previousValue;
  }
}

float TemperatureSensor::getAverageTemperature(int lookback) {
  return getAverageTemperature(lookback, 0);
}

// calculate the average temperature over the last (lookback) temperatures samples
float TemperatureSensor::getAverageTemperature(int lookback, int offsetReading) {
  float averageInput = 0;
  int count = 0;
  if (lookback >= numberOfReadings) lookback = numberOfReadings - 1;
  for (int offset = 0; offset < lookback; offset++) {
    int thisReading = (m_readIndex - offset - offsetReading) % numberOfReadings;
    if (thisReading < 0) thisReading += numberOfReadings;
    //DEBUG_print("getAverageTemperature(%d, %d): %d/%d = %0.2f\n", lookback, offsetReading, thisReading, m_readIndex, m_readingsTemp[thisReading]);
    if (m_readingsTime[thisReading] == 0) break;
    averageInput += m_readingsTemp[thisReading];
    count += 1;
  }
  if (count > 0) {
    return averageInput / count;
  } else {
    if (millis() > 60000) ERROR_print("getAverageTemperature(): no samples found\n");
    return 0;
  }
}

/********************************************************
 * history temperature data
 *****************************************************/
void TemperatureSensor::updateTemperatureHistory(float myInput) {
  m_readIndex++;
  if (m_readIndex >= numberOfReadings) {
    m_readIndex = 0;
  }
  m_readingsTime[m_readIndex] = millis();
  m_readingsTemp[m_readIndex] = myInput;
}

// calculate the temperature difference between NOW and a datapoint in history
float TemperatureSensor::pastTemperatureChange(int lookback) {
   return pastTemperatureChange(lookback, true);
}

float TemperatureSensor::pastTemperatureChange(int lookback, bool enable_avg) {
  // take 10samples (10*100ms = 1sec) for average calculations
  // thus lookback must be > avg_timeframe
  const int avg_timeframe = 10;  
  if (lookback >= numberOfReadings) lookback = numberOfReadings - 1;
  if (enable_avg) {
    int historicOffset = lookback - avg_timeframe;
    if (historicOffset < 0) return 0; //pastTemperatureChange will be 0 nevertheless
    float cur = getAverageTemperature(avg_timeframe);
    float past = getAverageTemperature(avg_timeframe, historicOffset);
    // ignore not yet initialized values
    if (cur == 0 || past == 0) return 0;
    return cur - past;
  } else {
    int historicIndex = (m_readIndex - lookback) % numberOfReadings;
    if (historicIndex < 0) { historicIndex += numberOfReadings; }
    // ignore not yet initialized values
    if (m_readingsTime[m_readIndex] == 0 || m_readingsTime[historicIndex] == 0) return 0;
    return m_readingsTemp[m_readIndex] - m_readingsTemp[historicIndex];
  }
}

float TemperatureSensor::getCurrentTemperature() {
  return m_readingsTemp[m_readIndex];
}

float TemperatureSensor::getTemperature(int lookback) {
  if (lookback >= numberOfReadings) lookback = numberOfReadings - 1;
  int offset = lookback % numberOfReadings;
  int historicIndex = (m_readIndex - offset);
  if (historicIndex < 0) { historicIndex += numberOfReadings; }
  // ignore not yet initialized values
  if (m_readingsTime[historicIndex] == 0) { return 0; }
  return m_readingsTemp[historicIndex];
}

float TemperatureSensor::getLatestTemperature() {
  return m_latestTemperature;  
}
