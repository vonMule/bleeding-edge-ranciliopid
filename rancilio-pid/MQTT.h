#ifndef _mqtt_H
#define _mqtt_H

#include <stdio.h>
#include <PubSubClient.h>

#ifdef ESP32
#include <os.h>
#endif

#if (BLYNK_ENABLE == 1)
#ifdef ESP32
#include <BlynkSimpleEsp32.h>
#else
#include <BlynkSimpleEsp8266.h>
#endif
extern BlynkWifi Blynk;
#endif

bool mqtt_reconnect(bool);
bool mqtt_publish(char*, char*);
bool mqtt_working();
void mqtt_publish_settings();

char* bool2string(bool in);
char* int2string(int state);
char* number2string(double in);
char* number2string(float in);

extern const char* mqtt_topic_prefix;
extern const char* hostname;
extern bool wifi_working();
extern PubSubClient mqtt_client;
extern bool force_offline;
extern bool mqtt_disabled_temporary;
extern unsigned long mqtt_dontPublishUntilTime;
extern const int MQTT_MAX_PUBLISH_SIZE;
extern const bool mqtt_flag_retained;
extern unsigned long mqtt_dontPublishBackoffTime;
extern bool in_sensitive_phase();
extern WiFiClient espClient;
extern unsigned long mqtt_lastReconnectAttemptTime;
extern unsigned long mqtt_reconnect_incremental_backoff;
extern unsigned int mqtt_reconnectAttempts;
extern unsigned long all_services_lastReconnectAttemptTime;
extern unsigned long all_services_min_reconnect_interval;
extern const char* mqtt_username;
extern const char* mqtt_password;
extern char topic_will[256];
extern char topic_set[256];
extern char topic_actions[256];
extern unsigned long mqtt_connectTime;
extern unsigned int mqtt_max_incremental_backoff;
extern double brewtime;
extern double preinfusion;
extern double preinfusionpause;
extern double starttemp;
extern double setPoint;
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
extern int steadyPowerOffsetTime;
extern double steadyPowerMQTTDisableUpdateUntilProcessed;
extern unsigned long steadyPowerMQTTDisableUpdateUntilProcessedTime;
extern double steadyPowerSaved;
extern unsigned long force_eeprom_sync;

#endif
