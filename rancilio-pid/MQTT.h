#ifndef _mqtt_H
#define _mqtt_H

#include <stdio.h>

#if (MQTT_ENABLE == 1)
#include <PubSubClient.h>
extern PubSubClient mqttClient;
#elif (MQTT_ENABLE == 2)
#include <uMQTTBroker.h>
extern bool MQTT_local_publish(char* topic, char* data, size_t data_length, uint8_t qos, uint8_t retain);
#endif

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

bool mqttReconnect(bool);
bool mqttPublish(char*, char*);
bool isMqttWorking();
void mqttPublishSettings();
bool persistSetting(char*, double*, char*);
bool persistSetting(char*, int*, char*);
bool persistSetting(char*, unsigned int*, char*);
void mqttParse(char*, char*);
void mqttParseActions(char* topic_str, char* data_str);

char* bool2string(bool in);
char* int2string(int state);
char* number2string(double in);
char* number2string(float in);

extern const char* mqttTopicPrefix;
extern const char* hostname;
extern bool isWifiWorking();

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
extern unsigned int steadyPowerOffsetTime;
extern double steadyPowerMQTTDisableUpdateUntilProcessed;
extern unsigned long steadyPowerMQTTDisableUpdateUntilProcessedTime;
extern double steadyPowerSaved;
extern unsigned long eepromForceSync;

#endif
