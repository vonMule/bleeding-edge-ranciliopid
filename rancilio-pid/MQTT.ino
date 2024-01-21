/********************************************************
 * Perfect Coffee PID
 * https://github.com/medlor/bleeding-edge-ranciliopid
 *****************************************************/
#include <float.h>
#include "userConfig.h"
#include "MQTT.h"
#include "rancilio-debug.h"
#include "rancilio-network.h"
#include "controls.h"
#include "rancilio-pid.h"  // refactoring needed to remove this

const char* mqttServerIP = MQTT_SERVER_IP;
const int mqttServerPort = MQTT_SERVER_PORT;
const char* mqttUsername = MQTT_USERNAME;
const char* mqttPassword = MQTT_PASSWORD;
const char* mqttTopicPrefix = MQTT_TOPIC_PREFIX;
const int mqttMaxPublishSize = 120;
char topicWill[256];
char topicSet[256];
char topicActions[256];
const bool mqttFlagRetained = true;
unsigned long mqttDontPublishUntilTime = 0;
unsigned long mqttDontPublishBackoffTime = 15000; // Failsafe: dont publish if there are errors for 15 seconds
unsigned long mqttLastReconnectAttemptTime = 0;
unsigned int mqttReconnectAttempts = 0;
unsigned long mqttReconnectIncrementalBackoff = 30000; // Failsafe: add 30sec to reconnect time after each
                                                        // connect-failure.
unsigned int mqttMaxIncrementalBackoff = 4; // At most backoff <mqtt_max_incremenatl_backoff>+1 *
                                            // (<mqttReconnectIncrementalBackoff>ms)
bool mqttDisabledTemporary = false;
unsigned long mqttConnectTime = 0; // time of last successfull mqtt connection
unsigned long lastCheckMQTT = 0;

//TODO loaded from rancilio-pid.cpp. needs refactoring (global ConfigClass, injection)
/*
extern float* activeSetPoint;
extern float* activeStartTemp;
extern float* activeBrewTime;
extern float* activePreinfusion;
extern float* activePreinfusionPause;
extern unsigned int* activeBrewTimeEndDetection;
extern float* activeScaleSensorWeightSetPoint;
extern int pidON;
extern char debugLine[200];
extern unsigned int profile;
extern float setPointSteam;
extern bool inSensitivePhase();
*/

//blynk.cpp
extern bool InitBlynk();
extern void blynkSave(char*);


// --------------------------------------------
#if (MQTT_ENABLE == 1)
WiFiClient espClient;
#include <PubSubClient.h>
PubSubClient mqttClient(espClient);
#elif MQTT_ENABLE == 2
#if defined(ESP8266)
#include <uMQTTBroker.h>
#elif defined(ESP32)
#include <PicoMQTT.h>

PicoMQTT::Server mqtt{};

void mqttLoop() { mqtt.loop(); }
#endif
#endif

bool almostEqual_exact(float a, float b) { return fabs(a - b) <= FLT_EPSILON; }
bool almostEqual(float a, float b) { return fabs(a - b) <= 0.0001; }

char* bool2string(bool in) {
  if (in) {
    return (char*)"1";
  } else {
    return (char*)"0";
  }
}
char int2string_int[7];
char* int2string(int state) {
  sprintf(int2string_int, "%d", state);
  return int2string_int;
}
char number2string_float[22];
char* number2string(float in) {
  snprintf(number2string_float, sizeof(number2string_float), "%0.2f", in);
  return number2string_float;
}
char number2string_int[22];
char* number2string(int in) {
  snprintf(number2string_int, sizeof(number2string_int), "%d", in);
  return number2string_int;
}
char number2string_uint[22];
char* number2string(unsigned int in) {
  snprintf(number2string_uint, sizeof(number2string_uint), "%u", in);
  return number2string_uint;
}

/* ------------------------------ */
bool isMqttWorking() {
   return isMqttWorking(false);
}
#if (MQTT_ENABLE == 0) // MQTT Disabled
bool mqttPublish(char* reading, char* payload) { return true; }
bool mqttReconnect(bool force_connect = false) { return true; }
bool isMqttWorking(bool refresh) { return false; }

/* ------------------------------ */
#elif (MQTT_ENABLE == 1) // MQTT Client
bool isMqttWorking(bool refresh) { 
  static bool val_mqtt = false;
  if (refresh || millis() > lastCheckMQTT + 100UL) {
    lastCheckMQTT = millis();
    val_mqtt = ((MQTT_ENABLE > 0) && (isWifiWorking()) && (mqttClient.connected()));
  }
  return val_mqtt;
}

bool mqttPublish(char* reading, char* payload) {
  if (!MQTT_ENABLE || forceOffline || mqttDisabledTemporary) return true;
  if (!isMqttWorking()) { return false; }
  char topic[mqttMaxPublishSize];
  snprintf(topic, mqttMaxPublishSize, "%s%s/%s", mqttTopicPrefix, hostname, reading);

  if (strlen(topic) + strlen(payload) >= mqttMaxPublishSize) {
    ERROR_print("mqttPublish() wants to send too much data (len=%u)\n", strlen(topic) + strlen(payload));
    return false;
  } else {
    unsigned long currentMillis = millis();
    if (currentMillis > mqttDontPublishUntilTime) {
      bool ret = mqttClient.publish(topic, payload, mqttFlagRetained);
      if (ret == false) {
        mqttDontPublishUntilTime = millis() + mqttDontPublishBackoffTime;
        ERROR_print("Error on publish. Wont publish the next %lu ms\n", mqttDontPublishBackoffTime);
        mqttClient.disconnect();
      }
      return ret;
    } else { // TODO test this code block later (faking an error)
      ERROR_print("Data not published (still for the next %lu ms)\n", mqttDontPublishUntilTime - currentMillis);
      return false;
    }
  }
}

bool mqttReconnect(bool force_connect = false) {
  if (!MQTT_ENABLE || forceOffline || mqttDisabledTemporary || isMqttWorking() || inSensitivePhase()) return true;

  unsigned long now = millis();
  if (force_connect
      || ((now > mqttLastReconnectAttemptTime + (mqttReconnectIncrementalBackoff * (mqttReconnectAttempts<=mqttMaxIncrementalBackoff? mqttReconnectAttempts:mqttMaxIncrementalBackoff)))
          && now > allServicesLastReconnectAttemptTime + allservicesMinReconnectInterval)) {
    mqttLastReconnectAttemptTime = now;
    allServicesLastReconnectAttemptTime = now;
    DEBUG_print("Connecting to mqtt ...\n");
    if (mqttClient.connect(hostname, mqttUsername, mqttPassword, topicWill, 0, 0, "unexpected exit") == true) {
      DEBUG_print("Connected to mqtt server\n");
#ifdef ESP32
      //espClient.setTimeout(2); // set timeout for socket connect()/write() to 2 seconds (default 5 seconds).
#else
      //espClient.setTimeout(2000); // set timeout for mqtt connect()/write() to 2 seconds (default 5 seconds).
#endif
      mqttPublish((char*)"events", (char*)"Connected to mqtt server");
      if (!mqttClient.subscribe(topicSet) || !mqttClient.subscribe(topicActions)) { ERROR_print("Cannot subscribe to topic\n"); }
      mqttLastReconnectAttemptTime = 0;
      mqttReconnectAttempts = 0;
      mqttConnectTime = millis();
    } else {
      DEBUG_print("Cannot connect to mqtt server (consecutive failures=#%u)\n", mqttReconnectAttempts);
      mqttReconnectAttempts++;
    }
  }
  return mqttClient.connected();
}

void mqttCallback1(char* topic, unsigned char* data, unsigned int length) {
  // DEBUG_print("Message arrived [%s]: %s\n", topic, (const char *)data);
  char topic_str[255];
  os_memcpy(topic_str, topic, sizeof(topic_str));
  topic_str[254] = '\0';
  char data_str[255];
  os_memcpy(data_str, data, length);
  data_str[length] = '\0';
  //DEBUG_print("MQTT: %s = %s\n", topic_str, data_str);
  const int ignore_retained_actions_after_reconnect = 20000;
  if (strstr(topic_str, "/actions/") != NULL) {
    if (millis() >= mqttConnectTime + ignore_retained_actions_after_reconnect) { mqttParseActions(topic_str, data_str); }
  } else if (strstr(topic_str, "/active") != NULL) {  //ignore profile dependent variables after reconnect
    if (millis() >= mqttConnectTime + ignore_retained_actions_after_reconnect) { mqttParse(topic_str, data_str); }
  } else {
    mqttParse(topic_str, data_str);
  }
}

/* ------------------------------ */
#elif (MQTT_ENABLE == 2)
bool isMqttWorking(bool refresh) { return ((MQTT_ENABLE > 0) && (isWifiWorking())); }

bool mqttPublish(char* reading, char* payload) {
  if (!MQTT_ENABLE || forceOffline || mqttDisabledTemporary) return true;
  char topic[mqttMaxPublishSize];
  snprintf(topic, mqttMaxPublishSize, "%s%s/%s", mqttTopicPrefix, hostname, reading);
  if (!isMqttWorking()) { return false; }
  if (strlen(topic) + strlen(payload) >= mqttMaxPublishSize) {
    ERROR_print("mqttPublish() wants to send too much data (len=%u)\n", strlen(topic) + strlen(payload));
    return false;
  } else {
    unsigned long currentMillis = millis();
    if (currentMillis > mqttDontPublishUntilTime) {
#if defined(ESP32)
      bool ret = mqtt.publish(topic, payload);
#else
      bool ret = MQTT_local_publish((unsigned char*)&topic, (unsigned char*)payload, strlen(payload), 1, 1);
#endif
      if (ret == false) {
        mqttDontPublishUntilTime = millis() + mqttDontPublishBackoffTime;
        ERROR_print("Error on publish. Wont publish the next %lu ms\n", mqttDontPublishBackoffTime);
        // mqttClient.disconnect();
        // MQTT_server_cleanupClientCons();
      }
      return ret;
    } else { // TODO test this code block later (faking an error)
      ERROR_print("Data not published (still for the next %lu ms)\n", mqttDontPublishUntilTime - currentMillis);
      return false;
    }
  }
}

bool mqttReconnect(bool force_connect = false) { return true; }

void mqtt_callback_2(uint32_t* client, const char* topic, uint32_t topic_len, const char* data, uint32_t length) {
  char topic_str[topic_len + 1];
  os_memcpy(topic_str, topic, topic_len);
  topic_str[topic_len] = '\0';
  char data_str[length + 1];
  os_memcpy(data_str, data, length);
  data_str[length] = '\0';
  // DEBUG_print("MQTT: %s = %s\n", topic_str, data_str);
  if (strstr(topic_str, "/actions/") != NULL) {
    mqttParseActions(topic_str, data_str);
  } else {
    mqttParse(topic_str, data_str);
  }
}

#endif

void mqttParseActions(char* topic_str, char* data_str) {
  char topic_pattern[255];
  char actionParsed[120];
  char* endptr;
  // DEBUG_print("mqttParseActions(%s, %s)\n", topic_str, data_str);
  snprintf(topic_pattern, sizeof(topic_pattern), "%s%s/actions/%%[^\\/]", mqttTopicPrefix, hostname);
  // DEBUG_print("topic_pattern=%s\n",topic_pattern);
  if (sscanf(topic_str, topic_pattern, &actionParsed) != 1) {
    DEBUG_print("Ignoring un-parsable topic (%s)\n", topic_str);
    return;
  }
  int action = convertActionToDefine(actionParsed);
  if (action == UNDEFINED_ACTION) {
    DEBUG_print("Ignoring topic (%s) because of unknown action(%s)\n", topic_str, actionParsed);
    return;
  }
  int data_int = (int)strtol(data_str, &endptr, 10);
  if (*endptr != '\0') {
    DEBUG_print("Ignoring topic (%s) because data(%s) cannot be convered to int\n", topic_str, data_str);
    return;
  }
  actionController(action, data_int, false);
}

void mqttParse(char* topic_str, char* data_str) {
  char topic_pattern[255];
  char configVar[120];
  char cmd[64];

  //DEBUG_print("mqttParse(%s, %s)\n", topic_str, data_str);
  snprintf(topic_pattern, sizeof(topic_pattern), "%s%s/%%[^\\/]/%%[^\\/]", mqttTopicPrefix, hostname);
  if ((sscanf(topic_str, topic_pattern, &configVar, &cmd) != 2) || (strcmp(cmd, "set") != 0)) {
    // DEBUG_print("Ignoring topic (%s)\n", topic_str);
    return;
  }
  if (strcmp(configVar, "profile") == 0) {
    if (persistSetting((char*)"profile", &profile, data_str)) {
      blynkSave((char*)"profile");
    }
    return;
  }
  if (strcmp(configVar, "activeBrewTime") == 0) {
    if (persistSetting((char*)"activeBrewTime", activeBrewTime, data_str)) {
      blynkSave((char*)"activeBrewTime");
    }
    return;
  }
  if (strcmp(configVar, "activeStartTemp") == 0) {
    if (persistSetting((char*)"activeStartTemp", activeStartTemp, data_str)) {
      blynkSave((char*)"activeStartTemp");
    }
    return;
  }
  if (strcmp(configVar, "activeSetPoint") == 0) {
    if (persistSetting((char*)"activeSetPoint", activeSetPoint, data_str)) {
      blynkSave((char*)"activeSetPoint");
    }
    return;
  }
  if (strcmp(configVar, "activePreinfusion") == 0) {
    if (persistSetting((char*)"activePreinfusion", activePreinfusion, data_str)) {
      blynkSave((char*)"activePreinfusion");
    }
    return;
  }
  if (strcmp(configVar, "activePreinfusionPause") == 0) {
    if (persistSetting((char*)"activePreinfusionPause", activePreinfusionPause, data_str)) {
      blynkSave((char*)"activePreinfusionPause");
    }
    return;
  }
  if (strcmp(configVar, "pidON") == 0) {
    if (persistSetting((char*)"pidON", &pidON, data_str)) {
      blynkSave((char*)"pidON");
    }
    pidON = pidON == 0 ? 0 : 1;
    return;
  }
  if (strcmp(configVar, "brewDetectionSensitivity") == 0) {
    if (persistSetting((char*)"brewDetectionSensitivity", &brewDetectionSensitivity, data_str)) {
      blynkSave((char*)"brewDetectionSensitivity");
    }
    return;
  }
  if (strcmp(configVar, "brewDetectionPower") == 0) {
    if (persistSetting((char*)"brewDetectionPower", &brewDetectionPower, data_str)) {
      blynkSave((char*)"brewDetectionPower");
    }
    return;
  }
  if (strcmp(configVar, "steadyPower") == 0) {
    if (persistSetting((char*)"steadyPower", &steadyPower, data_str)) {
      //blynkSave((char*)"steadyPower");  //handled every few seconds by another func
    }
    return;
  }
  if (strcmp(configVar, "steadyPowerOffset") == 0) {
    if (persistSetting((char*)"steadyPowerOffset", &steadyPowerOffset, data_str)) {
      blynkSave((char*)"steadyPowerOffset");
    }
    return;
  }
  if (strcmp(configVar, "steadyPowerOffsetTime") == 0) {
    if (persistSetting((char*)"steadyPowerOffsetTime", &steadyPowerOffsetTime, data_str)) {
      blynkSave((char*)"steadyPowerOffsetTime");
    }
    return;
  }
  if (strcmp(configVar, "aggKp") == 0) {
    if (persistSetting((char*)"aggKp", &aggKp, data_str)) {
      blynkSave((char*)"aggKp");
    }
    return;
  }
  if (strcmp(configVar, "aggTn") == 0) {
    if (persistSetting((char*)"aggTn", &aggTn, data_str)) {
      blynkSave((char*)"aggTn");
    }
    return;
  }
  if (strcmp(configVar, "aggTv") == 0) {
    if (persistSetting((char*)"aggTv", &aggTv, data_str)) {
      blynkSave((char*)"aggTv");
    }
    return;
  }
  if (strcmp(configVar, "aggoKp") == 0) {
    if (persistSetting((char*)"aggoKp", &aggoKp, data_str)) {
      blynkSave((char*)"aggoKp");
    }
    return;
  }
  if (strcmp(configVar, "aggoTn") == 0) {
    if (persistSetting((char*)"aggoTn", &aggoTn, data_str)) {
      blynkSave((char*)"aggoTn");
    }
    return;
  }
  if (strcmp(configVar, "aggoTv") == 0) {
    if (persistSetting((char*)"aggoTv", &aggoTv, data_str)) {
      blynkSave((char*)"aggoTv");
    }
    return;
  }
  if (strcmp(configVar, "setPointSteam") == 0) { // TOBIAS: update wiki (blynk address,..)
    if (persistSetting((char*)"setPointSteam", &setPointSteam, data_str)) {
      blynkSave((char*)"setPointSteam");
    }
    return;
  }
  if (strcmp(configVar, "activeBrewTimeEndDetection") == 0) {
    if (persistSetting((char*)"activeBrewTimeEndDetection", activeBrewTimeEndDetection, data_str)) {
      blynkSave((char*)"activeBrewTimeEndDetection");
    }
    return;
  }
  if (strcmp(configVar, "activeScaleSensorWeightSetPoint") == 0) {
    if (persistSetting((char*)"activeScaleSensorWeightSetPoint", activeScaleSensorWeightSetPoint, data_str)) {
      blynkSave((char*)"activeScaleSensorWeightSetPoint");
    }
    return;
  }
}

bool persistSetting(char* setting, float* value, char* data_str) {
  float data_float;
  sscanf(data_str, "%f", &data_float);
  if (strcmp(setting, "steadyPower") == 0 && almostEqual(data_float, steadyPowerMQTTDisableUpdateUntilProcessed)) {
    steadyPowerMQTTDisableUpdateUntilProcessed = 0;
    steadyPowerMQTTDisableUpdateUntilProcessedTime = 0;
  }
  if (!almostEqual(data_float, *value)) {
    //DEBUG_print("setting %s=%s (=%0.2f) (prev=%.2f)\n", setting, data_str, data_float, *value);
    *value = data_float;
    if (strcmp(setting, "steadyPower") == 0) {
      steadyPowerSaved = *value; // prevent an additional mqtt "/set" call
    } 
    mqttPublish(setting, data_str);
    eepromForceSync = millis();
    return true;
  }
  return false;
}

bool persistSetting(char* setting, int* value, char* data_str) {
  int data_int;
  sscanf(data_str, "%d", &data_int);
  if (data_int != *value) {
    // DEBUG_print("setting %s=%s (=%d) (prev=%d)\n", setting, data_str, data_int, *value);
    *value = data_int;
    mqttPublish(setting, data_str);
    eepromForceSync = millis();
    return true;
  }
  return false;
}

bool persistSetting(char* setting, unsigned int* value, char* data_str) {
  unsigned int data_int;
  sscanf(data_str, "%u", &data_int);
  if (data_int != *value) {
    *value = data_int;
    mqttPublish(setting, data_str);
    eepromForceSync = millis();
    return true;
  }
  return false;
}

void mqttPublishSettings() {
  mqttPublish((char*)"profile/set", number2string(profile));
  mqttPublish((char*)"activeBrewTime/set", number2string(*activeBrewTime));
  mqttPublish((char*)"activeStartTemp/set", number2string(*activeStartTemp));
  mqttPublish((char*)"activeSetPoint/set", number2string(*activeSetPoint));
  mqttPublish((char*)"activePreinfusion/set", number2string(*activePreinfusion));
  mqttPublish((char*)"activePreinfusionPause/set", number2string(*activePreinfusionPause));
  mqttPublish((char*)"pidON/set", number2string(pidON));
  mqttPublish((char*)"brewDetectionSensitivity/set", number2string(brewDetectionSensitivity));
  mqttPublish((char*)"brewDetectionPower/set", number2string(brewDetectionPower));
  mqttPublish((char*)"aggKp/set", number2string(aggKp));
  mqttPublish((char*)"aggTn/set", number2string(aggTn));
  mqttPublish((char*)"aggTv/set", number2string(aggTv));
  mqttPublish((char*)"aggoKp/set", number2string(aggoKp));
  mqttPublish((char*)"aggoTn/set", number2string(aggoTn));
  mqttPublish((char*)"aggoTv/set", number2string(aggoTv));
  mqttPublish((char*)"setPointSteam/set", number2string(setPointSteam));
  mqttPublish((char*)"steadyPowerOffset/set", number2string(steadyPowerOffset));
  mqttPublish((char*)"steadyPowerOffsetTime/set", number2string(steadyPowerOffsetTime));
  mqttPublish((char*)"activeBrewTimeEndDetection/set", number2string(*activeBrewTimeEndDetection));
  mqttPublish((char*)"activeScaleSensorWeightSetPoint/set", number2string(*activeScaleSensorWeightSetPoint));
  mqttPublish((char*)"steadyPower/set", number2string(steadyPower)); // this should be last in list
}

// Setup Mqtt (and also call initBlynk()) returns eeprom_force_read
bool InitMqtt() {
  bool eeprom_force_read = true;
#if (MQTT_ENABLE == 1)
  snprintf(topicWill, sizeof(topicWill), "%s%s/%s", mqttTopicPrefix, hostname, "will");
  snprintf(topicSet, sizeof(topicSet), "%s%s/+/%s", mqttTopicPrefix, hostname, "set");
  snprintf(topicActions, sizeof(topicActions), "%s%s/actions/+", mqttTopicPrefix, hostname);
  //mqttClient.setKeepAlive(3);      //activates mqttping keepalives (default 15)
  mqttClient.setSocketTimeout(2);  //sets application level timeout (default 15)
  mqttClient.setServer(mqttServerIP, mqttServerPort);
  mqttClient.setCallback(mqttCallback1);
  if (!mqttReconnect(true)) {
    if (DISABLE_SERVICES_ON_STARTUP_ERRORS) mqttDisabledTemporary = true;
    ERROR_print("Cannot connect to MQTT. Disabling...\n");
    // displaymessage(State::Undefined, "Cannot connect to MQTT", "");
    // delay(1000);
  } else {
    const bool useRetainedSettingsFromMQTT = true;
    if (useRetainedSettingsFromMQTT) {
      // read and use settings retained in mqtt and therefore dont use eeprom values
      eeprom_force_read = false;
      unsigned long started = millis();
          while (isMqttWorking(true) && (millis() < started + 4000)) // attention: delay might not
                                                              // be long enough over WAN
      {
        mqttClient.loop();
      }
      eepromForceSync = 0;
    }
  }
#elif (MQTT_ENABLE == 2)
  DEBUG_print("Starting MQTT service\n");
  const unsigned int max_subscriptions = 30;
  const unsigned int max_retained_topics = 30;
  const unsigned int mqtt_service_port = 1883;
  snprintf(topicSet, sizeof(topicSet), "%s%s/+/%s", mqttTopicPrefix, hostname, "set");
  snprintf(topicActions, sizeof(topicActions), "%s%s/actions/+", mqttTopicPrefix, hostname);
#if defined(ESP32)
  auto callback = [](const char * topic, const char * payload) {
        mqtt_callback_2(nullptr, topic, strlen(topic), payload, strlen(payload));
    };
  mqtt.subscribe(topicSet, callback);
  mqtt.subscribe(topicActions, callback);
  mqtt.begin();
  mqtt.loop();
#else
  MQTT_server_onData(mqtt_callback_2);
  if (MQTT_server_start(mqtt_service_port, max_subscriptions, max_retained_topics)) {
    if (!MQTT_local_subscribe((unsigned char*)topicSet, 0) || !MQTT_local_subscribe((unsigned char*)topicActions, 0)) {
      ERROR_print("Cannot subscribe to local MQTT service\n");
    }
  } else {
    if (DISABLE_SERVICES_ON_STARTUP_ERRORS) mqttDisabledTemporary = true;
    ERROR_print("Cannot create MQTT service. Disabling...\n");
    // displaymessage(State::Undefined, "Cannot create MQTT service", "");
    // delay(1000);
  }
#endif
#endif
         
  return InitBlynk() && eeprom_force_read;
}
