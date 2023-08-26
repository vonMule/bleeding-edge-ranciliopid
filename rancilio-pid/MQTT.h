#ifndef _mqtt_H
#define _mqtt_H

#include <stdio.h>
#ifdef ESP32
#include <os.h>
#endif

#if (MQTT_ENABLE == 1)
#include <PubSubClient.h>
extern PubSubClient mqttClient;
#elif (MQTT_ENABLE == 2 && defined(ESP8266))
#include <uMQTTBroker.h>
//bool MQTT_local_publish(char* topic, char* data, size_t data_length, uint8_t qos, uint8_t retain);
#endif

bool isWifiWorking();
bool InitMqtt();
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
extern unsigned long mqttDontPublishUntilTime;

#endif
