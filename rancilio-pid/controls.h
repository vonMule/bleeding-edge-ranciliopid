#ifndef _control_H
#define _control_H

//supported actions
#define UNDEFINED_ACTION  0
#define BREWING   1
#define HOTWATER  2
#define STEAMING  3
#define HEATER    4
#define PUMP      5
#define VALVE     6
#define CLEANING  7
#define TEMP_INC  8
#define TEMP_DEC  9

/*
 * TODO:
 * - Add MQTT Integration
 * - FIX STEAMING PID
 * - Add further functions
 * - Code cleanup
 */

typedef struct controlMap
{
      int gpio;
      char* portType;  //analog/digital
      char* type;      //trigger/switch
      int lowerBoundary;
      int upperBoundary;
      int value;
      int action;
      struct controlMap* nextControlMap;
} controlMap;

unsigned long previousCheckControls = 0;
#define FREQUENCYCHECKCONTROLS 300  // TOBIAS: change to 50 or 200? make dynamical!

// actionState contain the status (on/off/..) of each actions
#define MAX_NUM_ACTIONS 20
int actionState[MAX_NUM_ACTIONS];

// gpioLastAction contain the last known action executed (per gpio)
#define MAX_NUM_GPIO 35
int gpioLastAction[MAX_NUM_GPIO];

controlMap* parseControlsConfig();
void printControlsConfig(controlMap*);
void checkControls(controlMap*);

int simulatedBrewSwitch = 0;



#endif
