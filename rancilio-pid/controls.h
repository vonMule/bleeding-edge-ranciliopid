#ifndef _control_H
#define _control_H

#include "userConfig.h"
#include "MQTT.h"
#include "PIDBias.h"
#include "GpioCheck.h"


// supported actions
#define UNDEFINED_ACTION 0 // can also be used as NOOP
#define BREWING 1
#define HOTWATER 2
#define STEAMING 3
#define HEATER 4
#define PUMP 5
#define VALVE 6
#define CLEANING 7
#define MENU_INC 8
#define MENU_DEC 9
#define SLEEPING 10
#define MENU 11

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
  char* gpioCheck;
  struct controlMap* nextControlMap;
} controlMap;

typedef struct menuMapValue {
  char* type;  // type of real value 
  bool is_double_ptr;  //is it a ptr to a ptr pointing to the real value
  void* ptr; // ptr to real value
} menuMapValue;

typedef struct menuMap {
  char* type;           // CONFIG | ACTION 
  char* item;           // CONFIG=<Support setting to change> | ACTION=<Action_Name>
  int action;           // converted "item" to Action (default: UNDEFINED_ACTION)
  float valueStep;      // CONFIG=<step-size> | ACTION =<1|0>
  menuMapValue* value;  // dynamically updated on access
  char* unit;
  struct menuMap* nextMenuMap;
} menuMap;

unsigned long previousCheckControls = 0;
#define FREQUENCYCHECKCONTROLS 100 // TOBIAS: change to 50 or 200? make dynamical!

// actionState contain the status (on/off/..) of each actions
#define MAX_NUM_ACTIONS 20
int actionState[MAX_NUM_ACTIONS];

// gpioLastAction contain the last known action executed (per gpio)
#define MAX_NUM_GPIO 35
int gpioLastAction[MAX_NUM_GPIO];

controlMap* parseControlsConfig();
void debugControlHardware(controlMap* controlsConfig);
void printControlsConfig(controlMap*);
menuMap* parseMenuConfig();
void printMenuConfig(menuMap*);
menuMap* getMenuConfigPosition(menuMap* menuConfig, unsigned int menuPosition);  
void checkControls(controlMap*);
void actionController(int, int);
void actionController(int, int, bool);
void actionController(int, int, bool, bool);
void printMultiToggleConfig();
void configureControlsHardware(controlMap* controlsConfig);
int convertActionToDefine(char*);
int convertPortModeToDefine(char* portMode);
char* convertDefineToAction(int action);
void actionPublish(char*, unsigned int, int, bool, bool);
void sleepingAction(int);
void cleaningAction(int state);
void steamingAction(int state);
void hotwaterAction(int state);
void brewingAction(int state);
void menuAction(int state);
void menuIncAction(int state);
void menuDecAction(int state);

int simulatedBrewSwitch = 0;

extern unsigned long userActivity;
extern controlMap* controlsConfig;
extern menuMap* menuConfig;
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
extern unsigned int brewStatisticsAdditionalDisplayTime;
extern bool MaschineColdstartRunOnce;
extern float steadyPowerOffsetModified;
extern unsigned int menuPosition;
extern unsigned long previousTimerMenuCheck;
extern void blynkSave(char*);
extern unsigned int* activeBrewTimeEndDetection;
extern float* activeScaleSensorWeightSetPoint;
extern unsigned long brewTimer;
extern float currentWeight;

#endif
