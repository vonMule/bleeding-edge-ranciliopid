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

bool mqttReconnect(bool);
bool mqttPublish(char*, char*);
bool isMqttWorking();
bool isMqttWorking(bool refresh);
void mqttPublishSettings();
bool persistSetting(char*, float*, char*);
bool persistSetting(char*, int*, char*);
bool persistSetting(char*, unsigned int*, char*);
void mqttParse(char*, char*);
void mqttParseActions(char* topic_str, char* data_str);
void mqttCallback1(char* topic, unsigned char* data, unsigned int length);
void mqtt_callback_2(uint32_t* client, const char* topic, uint32_t topic_len, const char* data, uint32_t length);

char* bool2string(bool in);
char* int2string(int state);
char* number2string(float in);
char* number2string(float in);
char* number2string(int in);
char* number2string(unsigned int in);

extern const char* mqttTopicPrefix;
extern const char* hostname;
extern bool isWifiWorking();

void InitMqtt(bool);

extern bool forceOffline;
extern bool mqttDisabledTemporary;
extern unsigned long mqttDontPublishUntilTime;
extern const int mqttMaxPublishSize;
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
extern float* activeBrewTime;
extern float* activePreinfusion;
extern float* activePreinfusionPause;
extern float* activeStartTemp;
extern float* activeSetPoint;
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

#endif
