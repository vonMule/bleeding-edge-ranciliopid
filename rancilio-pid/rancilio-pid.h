#ifndef rancilio_pid_h
#define rancilio_pid_h

#define LIBRARY_VERSION 0.0.1

#include "userConfig.h"
#include "TemperatureSensor.h"

#ifdef ESP8266
#include <core_version.h>
#if defined(ARDUINO_ESP8266_RELEASE_2_6_3) || defined(ARDUINO_ESP8266_RELEASE_2_7_3) || defined(ARDUINO_ESP8266_RELEASE_2_7_4) || defined(ARDUINO_ESP8266_RELEASE_2_7_5)
#error ERROR Only esp8266 >=3.0.2 supported. 
#endif
#if (SCALE_SENSOR_ENABLE)
#error ERROR Scale not supported on esp8266.
#endif 
#endif

#if ESP32
#if ( defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR < 2) )
#error ERROR esp32 <2 no longer supported. Upgrade boards-manager to v2.0.4+ or use Platformio.
#endif 
#endif

#if (SCALE_SENSOR_ENABLE == 0)
#if (BREWTIME_END_DETECTION1 == 1)
#error ERROR BREWTIME_END_DETECTION1=1 is only supported when SCALE_SENSOR_ENABLE=1.
#endif
#if (BREWTIME_END_DETECTION2 == 1)
#error ERROR BREWTIME_END_DETECTION2=1 is only supported when SCALE_SENSOR_ENABLE=1.
#endif
#if (BREWTIME_END_DETECTION3 == 1)
#error ERROR BREWTIME_END_DETECTION3=1 is only supported when SCALE_SENSOR_ENABLE=1.
#endif
#endif

#if (ENABLE_CALIBRATION_MODE == 1)
#define DEBUGMODE
#undef BLYNK_ENABLE
#define BLYNK_ENABLE 0
#undef MQTT_ENABLE
#define MQTT_ENABLE 0
#endif

#ifdef ESP32
// ESP32 sometimes (after crash) is not able to connect to wifi. Workaround: set DISABLE_SERVICES_ON_STARTUP_ERRORS to 0
#undef DISABLE_SERVICES_ON_STARTUP_ERRORS
#define DISABLE_SERVICES_ON_STARTUP_ERRORS 0
#if (MQTT_ENABLE == 2)
#error ERROR Not supported to set MQTT_ENABLE=2 on ESP32
#endif

#endif

#ifndef CONTROLS_CONFIG
#define CONTROLS_CONFIG ""
#endif

#ifndef MENU_CONFIG
#define MENU_CONFIG ""
#endif

#ifndef TEMPSENSOR_BITWINDOW
#define TEMPSENSOR_BITWINDOW 125
#endif

bool almostEqual(float, float);
void print_settings();
extern char debugLine[200];
void maintenance();
void performance_check();
void set_profile();
void set_profile(bool);
void debugWaterLevelSensor();
bool inSensitivePhase();

// TODO build static global ConfigStore and move all global variables in there
extern float Input;
extern double Output;
extern float* activeSetPoint;
extern float* activeBrewTime;
extern float* activePreinfusion;
extern float* activePreinfusionPause;
extern float* activeStartTemp;
extern float setPointSteam;
extern int pidON;
extern unsigned int profile;
extern const unsigned int windowSize;
extern bool mqttDisabledTemporary;
extern unsigned int* activeBrewTimeEndDetection;
extern unsigned long allServicesLastReconnectAttemptTime;
extern unsigned long allservicesMinReconnectInterval;
extern float brewDetectionSensitivity;
extern float brewDetectionPower;
extern float aggoKp;
extern float aggoTn;
extern float aggoTv;
extern float aggKp;
extern float aggTn;
extern float aggTv;
extern float steadyPower;
extern float steadyPowerOffset;
extern unsigned int steadyPowerOffsetTime;
extern float steadyPowerMQTTDisableUpdateUntilProcessed;
extern unsigned long steadyPowerMQTTDisableUpdateUntilProcessedTime;
extern float steadyPowerSaved;
extern float* activeScaleSensorWeightSetPoint;
extern float scaleSensorWeightOffset;

extern unsigned int powerOffTimer;
extern int cleaningCycles;
extern int cleaningInterval;
extern int cleaningPause;
extern bool brewReady;
extern float marginOfFluctuation;
extern bool checkBrewReady(float setPoint, float marginOfFluctuation, int lookback);
extern int previousPowerOffTimer;
extern unsigned long lastBrewEnd;

extern unsigned int profile;
//extern unsigned int activeProfile;
extern float brewtime1;
extern float brewtime2;
extern float brewtime3;
extern float preinfusion1;
extern float preinfusion2;
extern float preinfusion3;
extern float preinfusionpause1;
extern float preinfusionpause2;
extern float preinfusionpause3;
extern float starttemp1;
extern float starttemp2;
extern float starttemp3;
extern float setPoint1;
extern float setPoint2;
extern float setPoint3;
extern float setPointSteam;
extern int pidON;
extern float brewDetectionSensitivity;
extern float brewDetectionPower;
extern float aggoKp;
extern float aggoTn;
extern float aggoTv;
extern float aggKp;
extern float aggTn;
extern float aggTv;
extern float steadyPower;
extern float steadyPowerOffset;
extern unsigned int steadyPowerOffsetTime;

extern float steadyPowerSaved;
extern unsigned long eepromForceSync;
//extern void blynkSave(char*);
extern unsigned int brewtimeEndDetection1;
extern unsigned int brewtimeEndDetection2;
extern unsigned int brewtimeEndDetection3;
extern float scaleSensorWeightSetPoint1;
extern float scaleSensorWeightSetPoint2;
extern float scaleSensorWeightSetPoint3;
extern float scaleSensorWeightOffset;

extern TemperatureSensor tempSensor;

#endif
