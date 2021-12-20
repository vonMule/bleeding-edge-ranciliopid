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

#define expectedEepromVersion 7 // EEPROM values are saved according to this versions layout. Increase
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
extern double brewtime1;
extern double brewtime2;
extern double brewtime3;
extern double preinfusion1;
extern double preinfusion2;
extern double preinfusion3;
extern double preinfusionpause1;
extern double preinfusionpause2;
extern double preinfusionpause3;
extern double starttemp1;
extern double starttemp2;
extern double starttemp3;
extern double setPoint1;
extern double setPoint2;
extern double setPoint3;
extern double setPointSteam;
extern int pidON;
extern double brewDetectionSensitivity;
extern double brewDetectionPower;
extern double aggoKp;
extern double aggoTn;
extern double aggoTv;
extern double aggKp;
extern double aggTn;
extern double aggTv;
extern double steadyPower;
extern double steadyPowerOffset;
extern unsigned int steadyPowerOffsetTime;
extern double steadyPowerMQTTDisableUpdateUntilProcessed;
extern unsigned long steadyPowerMQTTDisableUpdateUntilProcessedTime;
extern double steadyPowerSaved;
extern unsigned long eepromForceSync;
extern double burstPower;
extern void blynkSave(char*);

#endif