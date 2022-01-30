#ifndef _eeprom_pid_H
#define _eeprom_pid_H

#include "userConfig.h"

#ifdef ESP32
#include <Preferences.h>
#include <WiFi.h>
Preferences preferences;
#else
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#endif

void sync_eeprom();
void sync_eeprom(bool startup_read, bool force_read);

#define expectedEepromVersion 8 // EEPROM values are saved according to this versions layout. Increase
                                // if a new layout is implemented.

extern bool forceOffline;
extern bool mqttDisabledTemporary;
extern unsigned long mqttDontPublishUntilTime;
extern const int MQTT_MAX_PUBLISH_SIZE;
extern const bool mqttFlagRetained;
extern unsigned long mqttDontPublishBackoffTime;
extern bool inSensitivePhase();
extern WiFiClient espClient;
extern unsigned long mqttLastReconnectAttemptTime;
extern unsigned long mqttReconnectIncrementalBackoff;
extern unsigned int mqttReconnectAttempts;
extern unsigned long allServicesLastReconnectAttemptTime;
extern unsigned long allservicesMinReconnectInterval;
extern const char* mqttUsername;
extern const char* mqttPassword;
extern char topicWill[256];
extern char topicSet[256];
extern char topicActions[256];
extern unsigned long mqttConnectTime;
extern unsigned int mqttMaxIncrementalBackoff;
extern unsigned int profile;
extern unsigned int activeProfile;
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
extern float steadyPowerMQTTDisableUpdateUntilProcessed;
extern unsigned long steadyPowerMQTTDisableUpdateUntilProcessedTime;
extern float steadyPowerSaved;
extern unsigned long eepromForceSync;
extern void blynkSave(char*);
extern unsigned int brewtimeEndDetection1;
extern unsigned int brewtimeEndDetection2;
extern unsigned int brewtimeEndDetection3;
extern float scaleSensorWeightSetPoint1;
extern float scaleSensorWeightSetPoint2;
extern float scaleSensorWeightSetPoint3;

#endif