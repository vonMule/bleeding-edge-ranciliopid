/********************************************************
 * Perfect Coffee PID
 * https://github.com/medlor/bleeding-edge-ranciliopid
 *****************************************************/
#include "rancilio-network.h"
#include "userConfig.h"
#include "rancilio-debug.h"
#include "rancilio-enums.h"
#include "MQTT.h"
#include "blynk.h"
#include "display.h"

// Wifi
const char* hostname = HOSTNAME;
const char* ssid = D_SSID;
const char* pass = PASS;

unsigned long lastWifiConnectionAttempt = millis();
const unsigned long wifiReconnectInterval = 15000; // try to reconnect every 15 seconds
unsigned long wifiConnectWaitTime = 10000; // ms to wait for the connection to succeed
unsigned int wifiReconnects = 0; // number of reconnects

unsigned long lastCheckNetwork = 0;
bool forceOffline = FORCE_OFFLINE;


/******************************************************
 * WiFi helper scripts
 ******************************************************/
#ifdef ESP32
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  DEBUG_print("Connected to AP successfully\n");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  DEBUG_print("WiFi connected. IP=%s\n", WiFi.localIP().toString().c_str());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  ERROR_print("WiFi lost connection. (IP=%s)\n", WiFi.localIP().toString().c_str());
}
#endif


bool isWifiWorking() {
  static bool val_wifi = false;
  if (millis() - lastCheckNetwork > 100UL) {
    lastCheckNetwork = millis();
#ifdef ESP32
    //DEBUG_print("status=%d, IP=%s\n", WiFi.status() == WL_CONNECTED, WiFi.localIP().toString().c_str());
    val_wifi = ((!forceOffline) && (WiFi.status() == WL_CONNECTED)); // XXX1 correct to remove IPAddress(0) check?
#else
    val_wifi = ((!forceOffline) && (WiFi.status() == WL_CONNECTED) && (WiFi.localIP() != IPAddress(0U)));
#endif
}
  return val_wifi;
}

/********************************************************
 * Check if Wifi is connected, if not init&connect
 *****************************************************/
  void checkWifi(bool force_connect, unsigned long wifiConnectWaitTime_tmp, bool isInSensitivePhase) {
    if (forceOffline)
      return; // remove this to allow wifi reconnects even when
              // DISABLE_SERVICES_ON_STARTUP_ERRORS=1
    if ((!force_connect) && (isWifiWorking() || isInSensitivePhase)) return;

    if (force_connect || (millis() > lastWifiConnectionAttempt + 5000 + (wifiReconnectInterval * (wifiReconnects<=4?wifiReconnects: 4) ))) {
      // noInterrupts();
      DEBUG_print("Connecting to WIFI with SID %s ...\n", ssid);
      WiFi.persistent(false); // Don't save WiFi configuration in flash
      #ifdef ESP32
      WiFi.disconnect();
      WiFi.setHostname(hostname);
      #else
      WiFi.disconnect(true); // Delete SDK WiFi config
      #endif
// displaymessage(State::Undefined, "Connecting Wifi", "");
#ifdef STATIC_IP
      IPAddress STATIC_IP;
      IPAddress STATIC_GATEWAY;
      IPAddress STATIC_SUBNET;
      WiFi.config(ip, gateway, subnet);
#endif
      /* Explicitly set the ESP to be a WiFi-client, otherwise, it by
default, would try to act as both a client and an access-point and could cause
network-issues with your other WiFi-devices on your WiFi-network. */
      WiFi.mode(WIFI_STA);
      delay(200); // esp32: prevent "store calibration data failed(0x1105)" errors
#ifdef ESP32
      WiFi.setSleep(WIFI_PS_NONE);  //disable powersaving mode. improve network performance. disabled because sometimes reboot happen?!
#else
      WiFi.setSleepMode(WIFI_NONE_SLEEP); // needed for some disconnection bugs?
      WiFi.setAutoConnect(false); // disable auto-connect
#endif
      // WiFi.enableSTA(true);
      WiFi.setAutoReconnect(false); // disable auto-reconnect
#ifdef ESP32
      WiFi.begin(ssid, pass);
#else
    WiFi.hostname(hostname);
    WiFi.begin(ssid, pass);
#endif

      lastWifiConnectionAttempt = millis();
      while (!isWifiWorking() && (millis() < lastWifiConnectionAttempt + wifiConnectWaitTime_tmp)) {
        yield(); // Prevent Watchdog trigger
      }
      if (isWifiWorking()) {
        DEBUG_print("Wifi connection attempt (#%u) successfull (%lu secs)\n", wifiReconnects, (millis() - lastWifiConnectionAttempt) / 1000);
        wifiReconnects = 0;
        #if (MQTT_ENABLE == 2)
        #ifdef ESP32
        picoMQTTBroker.stop();
        #endif
        #endif
        InitMqtt();
      } else {
        ERROR_print("Wifi connection attempt (#%u) not successfull (%lu secs)\n", wifiReconnects, (millis() - lastWifiConnectionAttempt) / 1000);
        wifiReconnects++;
        WiFi.disconnect();
      }
    }
  }

void checkWifi(bool isInSensitivePhase) { checkWifi(false, wifiConnectWaitTime, isInSensitivePhase); }


/********************************************************
* Setup network which is WIFI, MQTT and Blynk. Returns eeprom_force_read
******************************************************/
bool InitNetworking() {
  if (forceOffline) {
    DEBUG_print("Staying offline due to forceOffline=1\n");
    return true;
  }

  checkWifi(true, 12000UL, false); // wait up to 12 seconds for connection

  if (!isWifiWorking()) {
    ERROR_print("Cannot connect to WIFI %s. Disabling WIFI\n", ssid);
    if (DISABLE_SERVICES_ON_STARTUP_ERRORS) {
      forceOffline = true;
      mqttDisabledTemporary = true;
      disableBlynkTemporary();
      lastWifiConnectionAttempt = millis();
    }
    displaymessage(State::Undefined, (char*)"Cannot connect to Wifi", (char*)"");
    delay(1000);
    return true;
  } else {
    DEBUG_print("IP address: %s\n", WiFi.localIP().toString().c_str());
#if (defined(ESP32) and defined(DEBUGMODE))
      WiFi.onEvent(WiFiStationConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
      WiFi.onEvent(WiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
      WiFi.onEvent(WiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP6);
      WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
#endif
    return InitMqtt();
  }
}
