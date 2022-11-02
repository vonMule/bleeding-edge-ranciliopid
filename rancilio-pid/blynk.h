#ifndef _blynk_H
#define _blynk_H

#define BLYNK_PRINT Serial
#ifdef ESP32
#include <BlynkSimpleEsp32.h>
#else
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#endif

#include <userConfig.h>

/********************************************************
 * BLYNK
 ******************************************************/
#define BLYNK_GREEN "#23C48E"
#define BLYNK_YELLOW "#ED9D00"
#define BLYNK_RED "#D3435C"
unsigned long previousTimerBlynk = 0;
unsigned long blynkConnectTime = 0;
const long intervalBlynk = 1000; // Update Intervall zur App
int blynkSendCounter = 1;
bool blynkSyncRunOnce = false;
String PreviousError = "";
String PreviousOutputString = "";
String PreviousPastTemperatureChange = "";
String PreviousInputString = "";
bool blynkDisabledTemporary = false;

float steadyPowerSavedInBlynk = 0;
unsigned long previousTimerBlynkHandle = 0;

// Blynk
const char* blynkAddress = BLYNKADDRESS;
const int blynkPort = BLYNKPORT;
const char* blynkAuth = BLYNKAUTH;
unsigned long blynkLastReconnectAttemptTime = 0;
unsigned int blynkReconnectAttempts = 0;
unsigned long blynkReconnectIncrementalBackoff = 180000; // Failsafe: add 180sec to reconnect time after each
                                                         // connect-failure.
unsigned int blynkMaxIncrementalBackoff = 5; // At most backoff <mqtt_max_incremenatl_backoff>+1 *
                                             // (<mqttReconnectIncrementalBackoff>ms)

char* blynkReadyLedColor = (char*)"#000000";

void blynkSave(char* setting);
void sendToBlynk();
bool setupBlynk();
void runBlynk();
void disableBlynkTemporary();
void setPreviousTimerBlynk(unsigned long prevTimer);


#endif