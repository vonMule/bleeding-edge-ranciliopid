#ifndef _control_H
#define _control_H

#include "userConfig.h"
#include "MQTT.h"
#include "PIDBias.h"


// supported actions
#define UNDEFINED_ACTION 0 // can also be used as NOOP
#define BREWING 1
#define HOTWATER 2
#define STEAMING 3
#define HEATER 4
#define PUMP 5
#define VALVE 6
#define CLEANING 7
#define TEMP_INC 8
#define TEMP_DEC 9
#define SLEEPING 10

#ifdef ESP32
#include "driver/rtc_io.h"
#endif

typedef struct controlMap {
  int gpio;
  char* portType; // analog/digital
  char* portMode; // INPUT_PULLUP/INPUT_PULLDOWN/INPUT
  char* type; // trigger/switch
  int lowerBoundary;
  int upperBoundary;
  int value;
  int action;
  struct controlMap* nextControlMap;
} controlMap;

unsigned long previousCheckControls = 0;
#define FREQUENCYCHECKCONTROLS 100 // TOBIAS: change to 50 or 200? make dynamical!

// actionState contain the status (on/off/..) of each actions
#define MAX_NUM_ACTIONS 20
int actionState[MAX_NUM_ACTIONS];

// gpioLastAction contain the last known action executed (per gpio)
#define MAX_NUM_GPIO 35
int gpioLastAction[MAX_NUM_GPIO];

controlMap* parseControlsConfig();
void printControlsConfig(controlMap*);
void checkControls(controlMap*);
void actionController(int, int);
void actionController(int, int, bool);
void actionController(int, int, bool, bool);
void printMultiToggleConfig();
void configureControlsHardware(controlMap* controlsConfig);
int convertActionToDefine(char*);

void mqttCallback1(char* topic, unsigned char* data, unsigned int length);
void mqtt_callback_2(uint32_t* client, const char* topic, uint32_t topic_len, const char* data, uint32_t length);

int simulatedBrewSwitch = 0;

extern unsigned long userActivity;
extern controlMap* controlsConfig;
extern const int OnlyPID;
extern const int brewDetection;
extern int brewing;
extern void setGpioAction(int action, bool mode);
extern int relayON, relayOFF;
extern int steaming;
extern int cleaning;
extern PIDBias bPID;
extern unsigned long userActivitySavedOnForcedSleeping;
extern int sleeping;
extern unsigned long lastBrewEnd;
extern bool MaschineColdstartRunOnce;
extern double steadyPowerOffsetModified;

#endif
