#ifndef RancilioPid_h
#define RancilioPid_h

#define LIBRARY_VERSION 0.0.1

#include "userConfig.h"

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

#define LCDWidth u8g2.getDisplayWidth()
#define LCDHeight u8g2.getDisplayHeight()
#define ALIGN_CENTER(t) ((LCDWidth - (u8g2.getUTF8Width(t))) / 2)
#define ALIGN_RIGHT(t) (LCDWidth - u8g2.getUTF8Width(t))
#define ALIGN_RIGHT_2(t1, t2) (LCDWidth - u8g2.getUTF8Width(t1) - u8g2.getUTF8Width(t2))
#define ALIGN_LEFT 0

// returns heater utilization in percent
float convertOutputToUtilisation(double);
// returns heater utilization in Output
double convertUtilisationToOutput(float);
float pastTemperatureChange(int);
float pastTemperatureChange(int, bool);
float getCurrentTemperature();
float readTemperatureFromSensor();
bool almostEqual(float, float);
void print_settings();
void checkWifi();
void checkWifi(bool, unsigned long);
extern char debugLine[200];
void maintenance();
void performance_check();
void set_profile();
void set_profile(bool);
void debugWaterLevelSensor();

extern void scaleCalibration();

#endif
