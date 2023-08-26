#ifndef _blynk_H
#define _blynk_H

#define BLYNK_PRINT Serial

#include "userConfig.h"
#include <WString.h>

/********************************************************
 * BLYNK
 ******************************************************/
#define BLYNK_GREEN "#23C48E"
#define BLYNK_YELLOW "#ED9D00"
#define BLYNK_RED "#D3435C"
extern unsigned long previousTimerBlynk;
extern unsigned long blynkConnectTime;
extern const long intervalBlynk;    // Update interval to send data to the app
extern int blynkSendCounter;
extern bool blynkSyncRunOnce;
extern bool blynkDisabledTemporary;
extern float steadyPowerSavedInBlynk;
extern unsigned long previousTimerBlynkHandle;

extern String PreviousError;
extern String PreviousOutputString;
extern String PreviousPastTemperatureChange;
extern String PreviousInputString;

// Blynk
extern const char* blynkAddress;
extern const int blynkPort;
extern const char* blynkAuth;
extern unsigned long blynkLastReconnectAttemptTime;
extern unsigned int blynkReconnectAttempts;
extern unsigned long blynkReconnectIncrementalBackoff; // Failsafe: add 180sec to reconnect time after each connect-failure.
extern unsigned int blynkMaxIncrementalBackoff; // At most backoff <mqtt_max_incremenatl_backoff>+1 * (<mqttReconnectIncrementalBackoff>ms)

extern char* blynkReadyLedColor;

bool isBlynkWorking();
void blynkSave(char* setting);
void sendToBlynk();
bool setupBlynk();
void runBlynk();
void disableBlynkTemporary();
void setPreviousTimerBlynk(unsigned long prevTimer);

/*
extern unsigned int profile;
extern float aggKp;
extern float aggTn;
extern float aggTv;
//extern float* activeSetPoint;
extern float* activeStartTemp;
//extern float* activeBrewTime;
extern float* activePreinfusion;
extern float* activePreinfusionPause;
extern int pidON;
extern float aggoKp;
extern float aggoTn;
extern float aggoTv;
extern float brewDetectionSensitivity;
extern float brewDetectionPower;
extern float steadyPower;
extern float steadyPowerOffset;
extern unsigned int steadyPowerOffsetTime;
extern float setPointSteam;
extern unsigned int* activeBrewTimeEndDetection;
extern float* activeScaleSensorWeightSetPoint;
extern float Input;
extern double Output;


*/

#endif
