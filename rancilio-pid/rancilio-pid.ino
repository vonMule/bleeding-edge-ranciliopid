/********************************************************
 * Perfect Coffee PID
 * https://github.com/medlor/bleeding-edge-ranciliopid
 *
 * The "old" origin can be found at http://rancilio-pid.de/
 *
 * In case of questions just contact, Tobias <medlor@web.de>
 *****************************************************/

#include <Arduino.h>
#include <ArduinoOTA.h>

#include <float.h>
#include <math.h>

#include "userConfig.h"
#include "rancilio-pid.h"
#include "MQTT.h"
#include "blynk.h"
#include "display.h"
#include "eeprom-pcpid.h"
#include "connection.h"
//#include "rancilio_debug.h"
#include "helper.h"
#include "TemperatureSensor.h"
#include "Enums.h"

RemoteDebug Debug;

const char* sysVersion PROGMEM = "3.2.3";

/********************************************************
 * definitions below must be changed in the userConfig.h file
 ******************************************************/
const int OnlyPID = ONLYPID;
const int TempSensorRecovery = TEMPSENSORRECOVERY;
const int brewDetection = BREWDETECTION;
const int valveTriggerType = VALVE_TRIGGERTYPE;
const int pumpTriggerType = PUMP_TRIGGERTYPE;

WiFiClient espClient;

// MQTT
#if (MQTT_ENABLE == 1)
#include <PubSubClient.h>
PubSubClient mqttClient(espClient);
#elif (MQTT_ENABLE == 2)
#include <uMQTTBroker.h>
#endif

const char* mqttServerIP = MQTT_SERVER_IP;
const char* mqttServerPort = MQTT_SERVER_PORT;
const char* mqttUsername = MQTT_USERNAME;
const char* mqttPassword = MQTT_PASSWORD;
const char* mqttTopicPrefix = MQTT_TOPIC_PREFIX;
const int mqttMaxPublishSize = MQTT_MAX_PUBLISH_SIZE;
char topicWill[256];
char topicSet[256];
char topicActions[256];
unsigned long lastMQTTStatusReportTime = 0;
unsigned long lastMQTTStatusReportInterval = 5000; // mqtt send status-report every 5 second
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

/********************************************************
 * states
 ******************************************************/
State activeState = State::InnerZoneDetected; // default state

/********************************************************
 * history of temperatures
 *****************************************************/
// const int numReadings = 75 * 10; // number of values per Array
// float readingsTemp[numReadings]; // the readings from Temp
// float readingsTime[numReadings]; // the readings from time
// int readIndex = 0; // the index of the current reading
unsigned long lastBrewTime = 0;
int timerBrewDetection = 0;

/********************************************************
 * PID Variables
 *****************************************************/
const unsigned int windowSizeSeconds = 5; // How often should PID.compute() run? must be >= 1sec
unsigned int windowSize = windowSizeSeconds * 1000; // 1000=100% heater power => resolution used in
                                                    // TSR() and PID.compute().
volatile unsigned int isrCounter = 0; // counter for heater ISR
#ifdef ESP32
hw_timer_t* timer = NULL;
#endif
const float heaterOverextendingFactor = 1.2;
unsigned int heaterOverextendingIsrCounter = windowSize * heaterOverextendingFactor;
unsigned long pidComputeLastRunTime = 0;
unsigned long streamComputeLastRunTime = 0;
float Input = 0;
double Output = 0;  // must be double: https://github.com/espressif/arduino-esp32/issues/3661
float secondlatestTemperature = 0;
double previousOutput = 0;
int pidMode = 1; // 1 = Automatic, 0 = Manual

float setPoint1 = SETPOINT1;
float setPoint2 = SETPOINT2;
float setPoint3 = SETPOINT3;
float* activeSetPoint = &setPoint1;
float starttemp1 = STARTTEMP1;
float starttemp2 = STARTTEMP2;
float starttemp3 = STARTTEMP3;
float* activeStartTemp = &starttemp1;
float setPointSteam = SETPOINT_STEAM;
float steamReadyTemp = STEAM_READY_TEMP;

// State 1: Coldstart PID values
const int coldStartStep1ActivationOffset = 15;
// ... none ...

// State 2: Coldstart stabilization PID values
// ... none ...

// State 3: Inner Zone PID values
float aggKp = AGGKP;
float aggTn = AGGTN;
float aggTv = AGGTV;
#if (aggTn == 0)
float aggKi = 0;
#else
float aggKi = aggKp / aggTn;
#endif
float aggKd = aggTv * aggKp;

// State 4: Brew PID values
// ... none ...
float brewDetectionPower = BREWDETECTION_POWER;

// State 5: Outer Zone Pid values
float aggoKp = AGGOKP;
float aggoTn = AGGOTN;
float aggoTv = AGGOTV;
#if (aggoTn == 0)
float aggoKi = 0;
#else
float aggoKi = aggoKp / aggoTn;
#endif
float aggoKd = aggoTv * aggoKp;
const float outerZoneTemperatureDifference = 1;
// const float steamZoneTemperatureDifference = 3;

/********************************************************
 * PID with Bias (steadyPower) Temperature Controller
 *****************************************************/
#include "PIDBias.h"
float steadyPower = STEADYPOWER; // in percent
float steadyPowerSaved = steadyPower;
float steadyPowerMQTTDisableUpdateUntilProcessed = 0; // used as semaphore
unsigned long steadyPowerMQTTDisableUpdateUntilProcessedTime = 0;
const int lastBrewTimeOffset = 4 * 1000; // compensate for lag in software brew-detection

// If the espresso hardware itself is cold, we need additional power for
// steadyPower to hold the water temperature
float steadyPowerOffset = STEADYPOWER_OFFSET; // heater power (in percent) which should be added to
                                               // steadyPower during steadyPowerOffsetTime
float steadyPowerOffsetModified = steadyPowerOffset;
unsigned int steadyPowerOffsetTime = STEADYPOWER_OFFSET_TIME; // timeframe (in s) for which
                                                              // steadyPowerOffsetActivateTime should be active
unsigned long steadyPowerOffsetActivateTime = 0;
unsigned long steadyPowerOffsetDecreaseTimer = 0;
unsigned long lastUpdateSteadyPowerOffset = 0; // last time steadyPowerOffset was updated
bool MaschineColdstartRunOnce = false;
bool MachineColdOnStart = true;
float starttempOffset = 0; // Increasing this lead to too high temp and emergency measures taking
                            // place. For my rancilio it is best to leave this at 0.

PIDBias bPID(&Input, &Output, &steadyPower, &steadyPowerOffsetModified, &steadyPowerOffsetActivateTime, &steadyPowerOffsetTime, &activeSetPoint, aggKp, aggKi, aggKd);

/********************************************************
 * BREWING / PREINFUSSION
 ******************************************************/
float brewtime1 = BREWTIME1;
float brewtime2 = BREWTIME2;
float brewtime3 = BREWTIME3;
float* activeBrewTime = &brewtime1;
float preinfusion1 = PREINFUSION1;
float preinfusion2 = PREINFUSION2;
float preinfusion3 = PREINFUSION3;
float* activePreinfusion = &preinfusion1;
float preinfusionpause1 = PREINFUSION_PAUSE1;
float preinfusionpause2 = PREINFUSION_PAUSE2;
float preinfusionpause3 = PREINFUSION_PAUSE3;
float* activePreinfusionPause = &preinfusionpause1;
unsigned int brewtimeEndDetection1 = BREWTIME_END_DETECTION1;
unsigned int brewtimeEndDetection2 = BREWTIME_END_DETECTION2;
unsigned int brewtimeEndDetection3 = BREWTIME_END_DETECTION3;
unsigned int* activeBrewTimeEndDetection = &brewtimeEndDetection1;
int brewing = 0; // Attention: "brewing" must only be changed in brew()
                 // (ONLYPID=0) or brewingAction() (ONLYPID=1)!
bool waitingForBrewSwitchOff = false;
int brewState = 0;
unsigned long lastBrewCheck = 0;
unsigned long totalBrewTime = 0;
unsigned long brewTimer = 0;
unsigned long brewStartTime = 0;
unsigned long previousBrewCheck = 0;
unsigned long lastBrewMessage = 0;

/********************************************************
 * STEAMING
 ******************************************************/
int steaming = 0;
unsigned long lastSteamMessage = 0;

/********************************************************
 * CLEANING
 ******************************************************/
int cleaning = 0; // this is the trigger
int cleaningEnableAutomatic = CLEANING_ENABLE_AUTOMATIC;
int cleaningCycles = CLEANING_CYCLES;
int cleaningInterval = CLEANING_INTERVAL;
int cleaningPause = CLEANING_PAUSE;
int cycle = 1;

/********************************************************
 * SLEEPING
 ******************************************************/
int sleeping = 0;
unsigned long previousTimerSleepCheck = 0;

/********************************************************
 * Rest
 *****************************************************/
unsigned int profile = 1;  // profile should be set
unsigned int activeProfile = profile;  // profile set

bool emergencyStop = false; // protect system when temperature is too high or sensor defect
int pidON = 1; // 1 = control loop in closed loop
int pumpRelayON, pumpRelayOFF; // used for pump relay trigger type. Do not change!
int valveRelayON, valveRelayOFF; // used for valve relay trigger type. Do not change!
char displayMessageLine1[21] = "\0";
char displayMessageLine2[21] = "\0";
unsigned long userActivity = 0;
unsigned long userActivitySavedOnForcedSleeping = 0;
unsigned long previousTimerRefreshTemp; // initialisation at the end of init()
unsigned long previousTimerMqttHandle = 0;

unsigned long previousTimerDebugHandle = 0;
unsigned long previousTimerPidCheck = 0;
#ifdef EMERGENCY_TEMP
const unsigned int emergencyTemperature = EMERGENCY_TEMP; // temperature at which the emergency shutdown should take
                                                          // place. DONT SET IT ABOVE 120 DEGREE!!
#else
const unsigned int emergencyTemperature = 120; // fallback
#endif
float brewDetectionSensitivity = BREWDETECTION_SENSITIVITY; // if temperature decreased within the last 6
                                                             // seconds by this amount, then we detect a
                                                             // brew.
#ifdef BREW_READY_DETECTION
const int enabledHardwareLed = ENABLE_HARDWARE_LED;
const int enabledHardwareLedNumber = ENABLE_HARDWARE_LED_NUMBER;
float marginOfFluctuation = float(BREW_READY_DETECTION);
#if (ENABLE_HARDWARE_LED == 2) // WS2812b based LEDs
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>
CRGB leds[enabledHardwareLedNumber];
#endif
#else
const int enabledHardwareLed = 0; // 0 = disable functionality
float marginOfFluctuation = 0; // 0 = disable functionality
#endif

unsigned long lastCheckBrewReady = 0;
unsigned long lastBrewReady = 0;
unsigned long lastBrewEnd = 0; // used to determime the time it takes to reach brewReady==true
unsigned long brewStatisticsTimer = 0;
unsigned long brewStatisticsAdditionalWeightTime = 2000;  //how many ms after brew() ends to still measure weight
unsigned int brewStatisticsAdditionalDisplayTime = 12000; //how many ms after brew() ends to show the brewStatistics
unsigned int powerOffTimer = 0;
bool brewReady = false;
unsigned long eepromSaveTimer = 28 * 60 * 1000UL; // save every 28min
unsigned long eepromSaveTime = 0;
char debugLine[200];
unsigned long recurringOutput = 0;
unsigned long allServicesLastReconnectAttemptTime = 0;
unsigned long allservicesMinReconnectInterval = 60000; // 60sec minimum wait-time between service reconnections
unsigned long eepromForceSync = 0;
const int eepromForceSyncWaitTimer = 3000; // after updating a setting wait this number of milliseconds before writing to eeprom
const int heaterInactivityTimer = HEATER_INACTIVITY_TIMER * 60 * 1000; // disable heater if no activity within the last minutes
int previousPowerOffTimer = 0; // in minutes
unsigned long loops = 0;
unsigned long maxMicros = 0;
unsigned long lastReportMicros = 0;
static unsigned long curMicros;
unsigned long curMicrosPreviousLoop = 0;
const unsigned long loopReportCount = 100;

/********************************************************
 * Water level sensor
 ******************************************************/
#if (WATER_LEVEL_SENSOR_ENABLE)
#include <VL53L0X.h>
#include <Wire.h>
VL53L0X waterSensor;
#endif
int waterSensorCheckTimer = 10000; // how often shall the water level be checked (in ms). must be >4000!
unsigned long previousTimerWaterLevelCheck = 0;

/********************************************************
 * Temperature Sensor 
 ******************************************************/
TemperatureSensor tempSensor(TempSensorRecovery);

// uint16_t temperature = 0;
// volatile uint16_t temp_value[2] = { 0 };
// volatile byte tsicDataAvailable = 0;

unsigned int isrCounterStripped = 0;
const int isrCounterFrame = 1000;

/********************************************************
 * SCALE
 ******************************************************/
#include "scale.h"
float scaleSensorWeightSetPoint1 = SCALE_SENSOR_WEIGHT_SETPOINT1;
float scaleSensorWeightSetPoint2 = SCALE_SENSOR_WEIGHT_SETPOINT2;
float scaleSensorWeightSetPoint3 = SCALE_SENSOR_WEIGHT_SETPOINT3;
float* activeScaleSensorWeightSetPoint = &scaleSensorWeightSetPoint1;
const int brewtimeMaxAdditionalTimeWhenWeightNotReached = 10; //in sec 
float scaleSensorWeightOffset = 1.5;  //automatic determined weight (gram) of dipping after brew has mechanically stopped
float scaleSensorWeightOffsetFactor = 0.3;
float scaleSensorWeightOffsetMax = 3;
float scaleSensorWeightOffsetMin = 0.05;
float scaleSensorWeightOffsetAtStop = 0;
unsigned long previousTimerScaleStatistics = 0;
unsigned long scaleSensorCheckTimer = 2000;

/********************************************************
 * CONTROLS
 ******************************************************/
#include "controls.h"
controlMap* controlsConfig = NULL;
unsigned long lastCheckGpio = 0;

/********************************************************
 * MENU
 ******************************************************/
unsigned int menuPosition = 0;
float menuValue = 0;
unsigned long previousTimerMenuCheck = 0;
const unsigned int menuOffTimer = 7000;
menuMap* menuConfig = NULL;

/******************************************************
 * HELPER
 ******************************************************/
void yieldIfNecessary(void){
    static uint64_t lastYield = 0;
    uint64_t now = millis();
    if((now - lastYield) > 15) {
        lastYield = now;
        delay(1);
        //vTaskDelay(5); //delay 1 RTOS tick
    }
}

bool inSensitivePhase() { return (brewing || activeState == State::BrewDetected || isrCounter > 1000); }


/********************************************************
 * Emergency Stop when temp too high
 *****************************************************/
void testEmergencyStop() {
  if (tempSensor.getCurrentTemperature() >= emergencyTemperature) {
    if (emergencyStop != true) {
      snprintf(debugLine, sizeof(debugLine), "EmergencyStop because temperature>%u (temperature=%0.2f)", emergencyTemperature, tempSensor.getCurrentTemperature());
      ERROR_println(debugLine);
      mqttPublish((char*)"events", debugLine);
      emergencyStop = true;
    }
  } else if (emergencyStop == true && tempSensor.getCurrentTemperature() < emergencyTemperature) {
    snprintf(debugLine, sizeof(debugLine), "EmergencyStop ended because temperature<%u (temperature=%0.2f)", emergencyTemperature, tempSensor.getCurrentTemperature());
    ERROR_println(debugLine);
    mqttPublish((char*)"events", debugLine);
    emergencyStop = false;
  }
}


// returns heater utilization in percent
float convertOutputToUtilisation(double Output) { return (100 * Output) / windowSize; }

// returns heater utilization in Output
double convertUtilisationToOutput(float utilization) { return (utilization / 100) * windowSize; }

bool checkBrewReady(float setPoint, float marginOfFluctuation, int lookback) {
  if (almostEqual(marginOfFluctuation, 0)) return false;
  if (lookback >= tempSensor.numberOfReadings) lookback = tempSensor.numberOfReadings - 1;
  for (int offset = 0; offset <= floor(lookback / 5); offset++) {
    int offsetReading = offset * 5;
    float temp_avg = tempSensor.getAverageTemperature(5, offsetReading);
    if (temp_avg == 0) return false;
    if (fabs(setPoint - temp_avg) > (marginOfFluctuation + FLT_EPSILON)) {
      return false;
    }
  }
  return true;
}

void setHardwareLed(bool mode) {
#if (ENABLE_HARDWARE_LED == 0)
  return;
#elif (ENABLE_HARDWARE_LED == 1)
  static bool previousMode = false;
  if (enabledHardwareLed == 1 && mode != previousMode) {
    digitalWrite(pinLed, mode);
    previousMode = mode;
  }
#elif (ENABLE_HARDWARE_LED == 2)
  static bool previousMode = false;
  if (enabledHardwareLed == 2 && mode != previousMode) {
    previousMode = mode;
    if (mode) {
      fill_solid(leds, enabledHardwareLedNumber, CRGB::ENABLE_HARDWARE_LED_RGB_ON);
    } else {
      fill_solid(leds, enabledHardwareLedNumber, CRGB::ENABLE_HARDWARE_LED_RGB_OFF);
    }
    FastLED.show();
  }
#endif
}

void setGpioAction(int action, bool mode) {
#if (ENABLE_GPIO_ACTION != 1)
  return;
#endif

#if defined(pinBrewAction) && defined(pinHotwaterAction) && defined(pinSteamingAction)
  static bool cleaningPreviousMode = false;
  if (action == CLEANING) {
    if (cleaningPreviousMode != mode) {
      cleaningPreviousMode = mode;
      digitalWrite(pinBrewAction, mode);
      digitalWrite(pinHotwaterAction, mode);
      digitalWrite(pinSteamingAction, mode);
    }
  }
#endif

  // no updates on GPIOActions when cleaning is active (leds shall always be on)
  if (cleaning) return;

#ifdef pinBrewAction
  static bool brewingPreviousMode = false;
  if (action == BREWING) {
    if (brewingPreviousMode != mode) {
      brewingPreviousMode = mode;
      digitalWrite(pinBrewAction, mode);
    }
  }
#endif

#ifdef pinHotwaterAction
  static bool hotwaterPreviousMode = false;
  if (action == HOTWATER) {
    if (hotwaterPreviousMode != mode) {
      hotwaterPreviousMode = mode;
      digitalWrite(pinHotwaterAction, mode);
    }
  }
#endif

#ifdef pinSteamingAction
  static bool steamingPreviousMode = false;
  if (action == STEAMING) {
    if (steamingPreviousMode != mode) {
      steamingPreviousMode = mode;
      digitalWrite(pinSteamingAction, mode);
    }
  }
#endif
}

  /********************************************************
   * Cleaning mode
   ******************************************************/
  void clean() {
    if (OnlyPID) { return; }
    unsigned long aktuelleZeit = millis();
    if (simulatedBrewSwitch && (brewing == 1 || waitingForBrewSwitchOff == false)) {
      totalBrewTime = (cleaningEnableAutomatic ? (cleaningInterval + cleaningPause) : 20) * 1000;
      if (brewing == 0) {
        brewing = 1; // Attention: For OnlyPID==0 brewing must only be changed
                     // in this function! Not externally.
        brewStartTime = aktuelleZeit;
        waitingForBrewSwitchOff = true;
      }
      brewTimer = aktuelleZeit - brewStartTime;
      if (aktuelleZeit >= lastBrewMessage + 1000) {
        lastBrewMessage = aktuelleZeit;
        DEBUG_print("clean(): time=%lu/%lu cycleCount=%d/%d\n", brewTimer / 1000, totalBrewTime / 1000, cycle, cleaningCycles);
      }
      if (brewTimer <= totalBrewTime) {
        if (!cleaningEnableAutomatic || brewTimer <= cleaningInterval * 1000U) {
          digitalWrite(pinRelayValve, valveRelayON);
          digitalWrite(pinRelayPump, pumpRelayON);
        } else {
          digitalWrite(pinRelayValve, valveRelayOFF);
          digitalWrite(pinRelayPump, pumpRelayOFF);
        }
      } else {
        if (cleaningEnableAutomatic && cycle < cleaningCycles) {
          brewStartTime = aktuelleZeit;
          cycle = cycle + 1;
        } else {
          DEBUG_print("End clean()\n");
          brewing = 0;
          cycle = 1;
          digitalWrite(pinRelayValve, valveRelayOFF);
          digitalWrite(pinRelayPump, pumpRelayOFF);
        }
      }
    } else if (simulatedBrewSwitch && !brewing) { // corner-case: switch=On but brewing==0
      waitingForBrewSwitchOff = true; // just to be sure
      brewTimer = 0;
    } else if (!simulatedBrewSwitch) {
      if (waitingForBrewSwitchOff) { DEBUG_print("simulatedBrewSwitch=off\n"); }
      waitingForBrewSwitchOff = false;
      if (brewing == 1) {
        digitalWrite(pinRelayValve, valveRelayOFF);
        digitalWrite(pinRelayPump, pumpRelayOFF);
        brewing = 0;
        cycle = 1;
      }
      brewTimer = 0;
    }
  }

  /********************************************************
   * PreInfusion, Brew , if not Only PID
   ******************************************************/
  void brew() {
    if (OnlyPID || (millis() < lastBrewCheck + 1) ) { return; }
    lastBrewCheck = millis();
    if (simulatedBrewSwitch && (brewing == 1 || waitingForBrewSwitchOff == false)) {

      if (brewing == 0) {
        brewing = 1; // Attention: For OnlyPID==0 brewing must only be changed in this function! Not externally.
        #if (SCALE_SENSOR_ENABLE)
        DEBUG_print("HX711: Powerup\n");
        scalePowerUp();
        //TODO tare is set to 0 but after 2 loops we have an weight offset of around 0.05g 
        //(looks like a minor bug in the hx771 lib when SAMPLES are low)
        tareAsync();
        #endif
        brewStartTime = millis();
        totalBrewTime = ( BREWTIME_TIMER == 1 ? *activePreinfusion + *activePreinfusionPause + *activeBrewTime : *activeBrewTime ) * 1000;
        flowRateEndTime = millis() + totalBrewTime + (brewtimeMaxAdditionalTimeWhenWeightNotReached * 1000);
        waitingForBrewSwitchOff = true; 
        scaleSensorWeightOffsetAtStop = 0;
      }
      brewTimer = millis() - brewStartTime;
      
      if (millis() >= lastBrewMessage + 1000) {
        brewStatisticsTimer = millis();  //refresh timer
        lastBrewMessage = millis();
        DEBUG_print("brew(%u): brewTimer=%02lu/%02lus (weight=%0.3fg/%0.2fg) (flowRateTimer=%ldms flowRate=%0.2fg/s)\n", 
          (*activeBrewTimeEndDetection >= 1 && getTareAsyncStatus()), brewTimer / 1000, totalBrewTime / 1000, currentWeight, *activeScaleSensorWeightSetPoint, 
          (flowRateEndTime - millis()), flowRate);
      }

      if (
        (brewTimer <= ((*activeBrewTimeEndDetection == 0 || !getTareAsyncStatus() ) ? totalBrewTime : (totalBrewTime + (brewtimeMaxAdditionalTimeWhenWeightNotReached * 1000))) ) && 
        ( (*activeBrewTimeEndDetection >= 1 && getTareAsyncStatus()) ? (currentWeight + scaleSensorWeightOffset < *activeScaleSensorWeightSetPoint) : true)
        && ( (*activeBrewTimeEndDetection >= 1 && getTareAsyncStatus()) ? (millis() <= flowRateEndTime) : true)
      ) {
        if (*activePreinfusion > 0 && brewTimer <= *activePreinfusion * 1000) {
          if (brewState != 1) {
            brewState = 1;
            //DEBUG_println("preinfusion");
            digitalWrite(pinRelayValve, valveRelayON);
            digitalWrite(pinRelayPump, pumpRelayON);
          }
        } else if (*activePreinfusion > 0 && brewTimer > *activePreinfusion * 1000 && brewTimer <= (*activePreinfusion + *activePreinfusionPause) * 1000) {
          if (brewState != 2) {
            brewState = 2;
            //DEBUG_println("preinfusion pause");
            digitalWrite(pinRelayValve, valveRelayON);
            digitalWrite(pinRelayPump, pumpRelayOFF);
          }
        } else if (*activePreinfusion == 0 || brewTimer > (*activePreinfusion + *activePreinfusionPause) * 1000) {
          if (brewState != 3) {
            brewState = 3;
            //DEBUG_println("brew");
            digitalWrite(pinRelayValve, valveRelayON);
            digitalWrite(pinRelayPump, pumpRelayON);
          }
        }
      } else {
        brewState = 0;
        //DEBUG_print("brew end\n");
        brewing = 0;
        digitalWrite(pinRelayValve, valveRelayOFF);
        digitalWrite(pinRelayPump, pumpRelayOFF);
        DEBUG_print("brew(%u): brewTimer=%02lu/%02lus (weight=%0.3fg/%0.2fg) (flowRateTimer=%ldms flowRate=%0.2fg/s)\n", 
          (*activeBrewTimeEndDetection >= 1 && getTareAsyncStatus()), brewTimer / 1000, totalBrewTime / 1000, currentWeight, *activeScaleSensorWeightSetPoint, 
          (flowRateEndTime - millis()), flowRate);
        flowRateEndTime = millis();  //dont restart brew due to weight flapping
        scaleSensorWeightOffsetAtStop = currentWeight ;
      }
    } else if (simulatedBrewSwitch && !brewing) { // corner-case: switch=On but brewing==0
      waitingForBrewSwitchOff = true; // just to be sure
      // digitalWrite(pinRelayValve, valveRelayOFF);  //already handled by brewing
      // var digitalWrite(pinRelayPump, pumpRelayOFF);
      brewState = 0;
    } else if (!simulatedBrewSwitch) {
      if (waitingForBrewSwitchOff) { DEBUG_print("simulatedBrewSwitch=off\n"); }
      waitingForBrewSwitchOff = false;
      if (brewing == 1) {
        digitalWrite(pinRelayValve, valveRelayOFF);
        digitalWrite(pinRelayPump, pumpRelayOFF);
        brewing = 0;
      }
      brewState = 0;
    }
  }


  /********************************************************
   * state Detection
   ******************************************************/
  void updateState() {
    switch (activeState) {
      case State::ColdStart: // state 1 running, that means full heater power. Check if target temp is reached
      {
        if (!MaschineColdstartRunOnce) {
          MaschineColdstartRunOnce = true;
          const int machineColdStartLimit = 45;
          if (Input <= *activeStartTemp && Input >= machineColdStartLimit) { // special auto-tuning settings
                                                                      // when maschine is already warm
            MachineColdOnStart = false;
            steadyPowerOffsetDecreaseTimer = millis();
            steadyPowerOffsetModified /= 2; // OK
            snprintf(debugLine, sizeof(debugLine), "steadyPowerOffset halved because maschine is already warm");
          }
        }
        bPID.SetFilterSumOutputI(100);
        if (Input >= *activeStartTemp + starttempOffset || !pidMode || steaming || sleeping || cleaning) { // 80.5 if 44C. | 79,7 if 30C |
          snprintf(debugLine, sizeof(debugLine),
              "** End of Coldstart. Transition to state 2 (constant steadyPower)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetSumOutputI(0);
          activeState = State::StabilizeTemperature;
        }
        break;
      }
      case State::StabilizeTemperature: // that means heater is on steadyState and we are waiting to temperature to stabilize
      {
        bPID.SetFilterSumOutputI(30);

        if ((Input - *activeSetPoint >= 0) || (Input - *activeSetPoint <= -20) || (Input - *activeSetPoint <= 0 && tempSensor.pastTemperatureChange(20*10) <= 0.3)
            || (Input - *activeSetPoint >= -1.0 && tempSensor.pastTemperatureChange(10*10) > 0.2) || (Input - *activeSetPoint >= -1.5 && tempSensor.pastTemperatureChange(10*10) >= 0.45) || !pidMode || sleeping
            || cleaning) {
          // auto-tune starttemp
          if (millis() < 400000 && steadyPowerOffsetActivateTime > 0 && pidMode && MachineColdOnStart && !steaming && !sleeping
              && !cleaning) { // ugly hack to only adapt setPoint after power-on
            float tempChange = tempSensor.pastTemperatureChange(10*10);
            if (Input - *activeSetPoint >= 0) {
              if (tempChange > 0.05 && tempChange <= 0.15) {
                DEBUG_print("Auto-Tune starttemp(%0.2f -= %0.2f) | "
                            "steadyPowerOffset=%0.2f | steadyPowerOffsetTime=%u\n",
                    *activeStartTemp, 0.5, steadyPowerOffset, steadyPowerOffsetTime);
                *activeStartTemp -= 0.5;
              } else if (tempChange > 0.15) {
                DEBUG_print("Auto-Tune starttemp(%0.2f -= %0.2f) | "
                            "steadyPowerOffset=%0.2f | steadyPowerOffsetTime=%u\n",
                    *activeStartTemp, 1.0, steadyPowerOffset, steadyPowerOffsetTime);
                *activeStartTemp -= 1;
              }
            } else if (Input - *activeSetPoint >= -1.5 && tempChange >= 0.8) { //
              DEBUG_print("Auto-Tune starttemp(%0.2f -= %0.2f, too fast) | "
                          "steadyPowerOffset=%0.2f | steadyPowerOffsetTime=%u\n",
                  *activeStartTemp, 0.4, steadyPowerOffset, steadyPowerOffsetTime);
              *activeStartTemp -= 0.4;
            } else if (Input - *activeSetPoint >= -1.5 && tempChange >= 0.45) { // OK (-0.10)!
              DEBUG_print("Auto-Tune starttemp(%0.2f -= %0.2f, too fast) | "
                          "steadyPowerOffset=%0.2f | steadyPowerOffsetTime=%u\n",
                  *activeStartTemp, 0.2, steadyPowerOffset, steadyPowerOffsetTime);
              *activeStartTemp -= 0.2;
            } else if (Input - *activeSetPoint >= -1.0 && tempChange > 0.2) { // OK (+0.10)!
              DEBUG_print("Auto-Tune starttemp(%0.2f -= %0.2f, too fast) | "
                          "steadyPowerOffset=%0.2f | steadyPowerOffsetTime=%u\n",
                  *activeStartTemp, 0.1, steadyPowerOffset, steadyPowerOffsetTime);
              *activeStartTemp -= 0.1;
            } else if (Input - *activeSetPoint <= -1.2) {
              DEBUG_print("Auto-Tune starttemp(%0.2f += %0.2f) | "
                          "steadyPowerOffset=%0.2f | steadyPowerOffsetTime=%u\n",
                  *activeStartTemp, 0.3, steadyPowerOffset, steadyPowerOffsetTime);
              *activeStartTemp += 0.3;
            } else if (Input - *activeSetPoint <= -0.6) {
              DEBUG_print("Auto-Tune starttemp(%0.2f += %0.2f) | "
                          "steadyPowerOffset=%0.2f | steadyPowerOffsetTime=%u\n",
                  *activeStartTemp, 0.2, steadyPowerOffset, steadyPowerOffsetTime);
              *activeStartTemp += 0.2;
            } else if (Input - *activeSetPoint >= -0.4) {
              DEBUG_print("Auto-Tune starttemp(%0.2f -= %0.2f) | "
                          "steadyPowerOffset=%0.2f | steadyPowerOffsetTime=%u\n",
                  *activeStartTemp, 0.1, steadyPowerOffset, steadyPowerOffsetTime);
              *activeStartTemp -= 0.1;
            }
            // persist starttemp auto-tuning setting
            mqttPublish((char*)"starttemp/set", number2string(*activeStartTemp));
            mqttPublish((char*)"starttemp", number2string(*activeStartTemp));
            blynkSave((char*)"starttemp");
            eepromForceSync = millis();
          } else {
            DEBUG_print("Auto-Tune starttemp disabled\n");
          }

          snprintf(debugLine, sizeof(debugLine), "** End of stabilizing. Transition to state 3 (normal mode)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetSumOutputI(0);
          activeState = State::InnerZoneDetected;
          bPID.SetAutoTune(true);
        }
        break;
      }
      case State::BrewDetected: // = Brew running
      {
        bPID.SetFilterSumOutputI(100);
        bPID.SetAutoTune(false);
        if (!brewing || (OnlyPID && brewDetection == 2 && brewTimer >= lastBrewTimeOffset + 3 && (brewTimer >= *activeBrewTime * 1000 || *activeSetPoint - Input < 0))) {
          if (OnlyPID && brewDetection == 2) brewing = 0;
          snprintf(debugLine, sizeof(debugLine), "** End of Brew. Transition to state 2 (constant steadyPower)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetAutoTune(true); // dont change mode during cleaning
          mqttPublish((char*)"brewDetected", (char*)"0");
          bPID.SetSumOutputI(0);
          lastBrewEnd = millis();  //used to detect time from brew until brewReady
          timerBrewDetection = 0;
          activeState = State::StabilizeTemperature; 
        }
        break;
      }
      case State::OuterZoneDetected: // state 5 in outerZone
      {
        if (Input >= *activeSetPoint - outerZoneTemperatureDifference - 1.5) {
          bPID.SetFilterSumOutputI(4.5);
        } else {
          bPID.SetFilterSumOutputI(9);
        }

        if (fabs(Input - *activeSetPoint) < outerZoneTemperatureDifference || steaming || (OnlyPID && brewDetection == 1 && simulatedBrewSwitch) || !pidMode || sleeping
            || cleaning) {
          snprintf(debugLine, sizeof(debugLine), "** End of outerZone. Transition to state 3 (normal mode)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          if (pidMode == 1) bPID.SetMode(AUTOMATIC);
          bPID.SetSumOutputI(0);
          bPID.SetAutoTune(true);
          timerBrewDetection = 0;
          activeState = State::InnerZoneDetected;
        }
        break;
      }
      case State::SteamMode: // state 6 heat up because we want to steam
      {
        bPID.SetAutoTune(false); // do not tune during steam phase

        if (!steaming) {
          snprintf(debugLine, sizeof(debugLine),
              "** End of Steaming phase. Now cooling down. Transition to state 3 (normal mode)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          set_profile(true);  
          //if (*activeSetPoint != setPoint) {
          //  activeSetPoint = &setPoint; // TOBIAS rename setPoint -> brewSetPoint
          //  DEBUG_print("set activeSetPoint: %0.2f\n", *activeSetPoint);
          //}
          if (pidMode == 1) bPID.SetMode(AUTOMATIC);
          bPID.SetSumOutputI(0);
          bPID.SetAutoTune(false);
          Output = 0;
          timerBrewDetection = 0;
          activeState = State::InnerZoneDetected;
        }
        break;
      }
      case State::SleepMode: // state 7 sleep modus activated (no heater,..)
      {
        if (!sleeping) {
          snprintf(debugLine, sizeof(debugLine), "** End of Sleeping phase. Transition to state 3 (normal mode)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetAutoTune(true);
          bPID.SetMode(AUTOMATIC);
          activeState = State::InnerZoneDetected;
        }
        break;
      }
      case State::CleanMode: // state 8 clean modus activated
      {
        if (!cleaning) {
          snprintf(debugLine, sizeof(debugLine), "** End of Cleaning phase. Transition to state 3 (normal mode)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetAutoTune(true);
          activeState = State::InnerZoneDetected;
        }
        break;
      }

      case State::InnerZoneDetected: // normal PID mode
      default: {
        if (!pidMode) break;

        // set maximum allowed filterSumOutputI based on
        // error/marginOfFluctuation
        if (Input >= *activeSetPoint - marginOfFluctuation) {
          if (bPID.GetFilterSumOutputI() != 1.0) { bPID.SetFilterSumOutputI(0); }
          bPID.SetFilterSumOutputI(1.0);
        } else if (Input >= *activeSetPoint - 0.5) {
          bPID.SetFilterSumOutputI(2.0);
        } else {
          bPID.SetFilterSumOutputI(4.5);
        }

        /* STATE 7 (SLEEP) DETECTION */
        if (sleeping) {
          snprintf(debugLine, sizeof(debugLine), "** End of normal mode. Transition to state 7 (sleeping)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          activeState = State::SleepMode;
          bPID.SetAutoTune(false);
          bPID.SetMode(MANUAL);
          break;
        }

        /* STATE 8 (Clean) DETECTION */
        if (cleaning) {
          snprintf(debugLine, sizeof(debugLine), "Cleaning Detected. Transition to state 8 (Clean)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetAutoTune(false); // do not tune
          activeState = State::CleanMode;
          break;
        }

        /* STATE 6 (Steam) DETECTION */
        if (steaming) {
          snprintf(debugLine, sizeof(debugLine), "Steaming Detected. Transition to state 6 (Steam)");
          // digitalWrite(pinRelayValve, valveRelayOFF);
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          if (*activeSetPoint != setPointSteam) {
            activeSetPoint = &setPointSteam;
            DEBUG_print("set activeSetPoint: %0.2f\n", *activeSetPoint);
          }
          activeState = State::SteamMode;
          break;
        }

        /* STATE 1 (COLDSTART) DETECTION */
        if (Input <= *activeStartTemp - coldStartStep1ActivationOffset) {
          snprintf(debugLine, sizeof(debugLine), "** End of normal mode. Transition to state 1 (coldstart)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          steadyPowerOffsetActivateTime = millis();
          DEBUG_print("Enable steadyPowerOffset (%0.2f)\n", steadyPowerOffset);
          bPID.SetAutoTune(false); // do not tune during coldstart + phase2
          bPID.SetSumOutputI(0);
          activeState = State::ColdStart;
          break;
        }

        /* STATE 4 (BREW) DETECTION */
        if (brewDetection == 1 || (brewDetectionSensitivity != 0 && brewDetection == 2)) {
          // enable brew-detection if not already running and diff temp is >
          // brewDetectionSensitivity
          if (brewing
              || (OnlyPID && brewDetection == 2 && (tempSensor.pastTemperatureChange(3*10) <= -brewDetectionSensitivity) && fabs(tempSensor.getTemperature(5*10) - *activeSetPoint) <= outerZoneTemperatureDifference
                  && millis() - lastBrewTime >= BREWDETECTION_WAIT * 1000)) {
            if (OnlyPID) {
              brewTimer = 0;
              if (brewDetection == 2) {
                brewing = 1;
                lastBrewTime = millis() - lastBrewTimeOffset;
              } else {
                lastBrewTime = millis() - 200;
              }
            }
            userActivity = millis();
            timerBrewDetection = 1;
            mqttPublish((char*)"brewDetected", (char*)"1");
            snprintf(debugLine, sizeof(debugLine), "** End of normal mode. Transition to state 4 (brew)");
            DEBUG_println(debugLine);
            mqttPublish((char*)"events", debugLine);
            bPID.SetSumOutputI(0);
            activeState = State::BrewDetected;
            break;
          }
        }

        /* STATE 5 (OUTER ZONE) DETECTION */
        if (Input > *activeStartTemp - coldStartStep1ActivationOffset && (fabs(Input - *activeSetPoint) > outerZoneTemperatureDifference) && !cleaning) {
          snprintf(debugLine, sizeof(debugLine), "** End of normal mode. Transition to state 5 (outerZone)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetSumOutputI(0);
          activeState = State::OuterZoneDetected;
          if (Input > *activeSetPoint) { // if we are above setPoint always disable heating (primary useful after steaming)  YYY1
            bPID.SetAutoTune(false);
            bPID.SetMode(MANUAL);
          }
          break;
        }

        break;
      }
    }

    // steadyPowerOffsetActivateTime handling
    if (steadyPowerOffsetActivateTime > 0) {
      if (Input - *activeSetPoint >= 1) {
        steadyPowerOffsetActivateTime = 0;
        snprintf(debugLine, sizeof(debugLine),
            "ATTENTION: Disabled steadyPowerOffset because its too large or starttemp too high");
        ERROR_println(debugLine);
        mqttPublish((char*)"events", debugLine);
        bPID.SetAutoTune(true);
      } else if (Input - *activeSetPoint >= 0.4 && millis() >= steadyPowerOffsetDecreaseTimer + 90000) {
        steadyPowerOffsetDecreaseTimer = millis();
        steadyPowerOffsetModified /= 2;
        snprintf(debugLine, sizeof(debugLine),
            "ATTENTION: steadyPowerOffset halved because its too large or starttemp too high");
        ERROR_println(debugLine);
        mqttPublish((char*)"events", debugLine);
      } else if (millis() >= steadyPowerOffsetActivateTime + steadyPowerOffsetTime * 1000) {
        steadyPowerOffsetActivateTime = 0;
        DEBUG_print("Disable steadyPowerOffset\n");
        bPID.SetAutoTune(true);
      }
    }
  }

  /***********************************
   * PID & HEATER ISR
   ***********************************/
  unsigned long pidComputeDelay = 0;
  float Output_save;
  void pidCompute() {
    // certain activeState set Output to fixed values
    if (activeState == State::ColdStart || activeState == State::StabilizeTemperature || activeState == State::BrewDetected) { Output_save = Output; }
    float pastChange = tempSensor.pastTemperatureChange(10*10) / 2; // difference of the last 10 seconds scaled down to one compute() cycle (=5 seconds).
    float pastChangeOverLongTime = tempSensor.pastTemperatureChange(20*10);  //20sec
    int ret = bPID.Compute(pastChange, pastChangeOverLongTime);
    if (ret == 1) { // compute() did run successfully
      if (isrCounter > (windowSize + 100)) {
        ERROR_print("pidCompute() delay: isrCounter=%d, "
                    "heaterOverextendingIsrCounter=%d, heater=%d\n",
            isrCounter, heaterOverextendingIsrCounter, digitalRead(pinRelayHeater));
      }
      isrCounter = 0; // Attention: heater might not shutdown if bPid.SetSampleTime(),
                      // windowSize, timer1_write() and are not set correctly!
      pidComputeDelay = millis() + 5 - pidComputeLastRunTime - windowSize;
      if (pidComputeDelay > 50 && pidComputeDelay < 100000000) { DEBUG_print("pidCompute() delay of %lu ms (loop() hang?)\n", pidComputeDelay); }
      pidComputeLastRunTime = millis();
      if (activeState == State::ColdStart || activeState == State::StabilizeTemperature || activeState == State::BrewDetected) {
#pragma GCC diagnostic error "-Wuninitialized"
        Output = Output_save;
      }
      DEBUG_print("Input=%6.2f | error=%5.2f delta=%5.2f | Output=%6.2f = b:%5.2f + "
                  "p:%5.2f + i:%5.2f(%5.2f) + d:%5.2f (RSSI=%d)\n",
          Input, (*activeSetPoint - Input), tempSensor.pastTemperatureChange(10*10) / 2, convertOutputToUtilisation(Output), steadyPower + bPID.GetSteadyPowerOffsetCalculated(),
          convertOutputToUtilisation(bPID.GetOutputP()), convertOutputToUtilisation(bPID.GetSumOutputI()), convertOutputToUtilisation(bPID.GetOutputI()),
          convertOutputToUtilisation(bPID.GetOutputD()), WiFi.RSSI());
    } else if (ret == 2) { // PID is disabled but compute() should have run
      isrCounter = 0;
      pidComputeLastRunTime = millis();
      DEBUG_print("Input=%6.2f | error=%5.2f delta=%5.2f | Output=%6.2f (PID "
                  "disabled)\n",
          Input, (*activeSetPoint - Input), tempSensor.pastTemperatureChange(10*10) / 2, convertOutputToUtilisation(Output));
    }
  }

#ifdef ESP32
  void IRAM_ATTR onTimer1ISR() {
    timerAlarmWrite(timer, 10000, true); // 10ms
    if (isrCounter >= heaterOverextendingIsrCounter) {
      // turn off when when compute() is not run in time (safetly measure)
      digitalWrite(pinRelayHeater, LOW);
      // ERROR_print("onTimer1ISR has stopped heater because pid.Compute() did not run\n");
      // TODO: add more emergency handling?
    } else if (isrCounter > windowSize) {
      // dont change output when overextending within overextending_factor threshold 
      // DEBUG_print("onTimer1ISR over extending due to processing delays: isrCounter=%u\n", isrCounter);
    } else if (isrCounter >= Output) { // max(Output) = windowSize
      digitalWrite(pinRelayHeater, LOW);
    } else {
      digitalWrite(pinRelayHeater, HIGH);
    }
    if (isrCounter <= (heaterOverextendingIsrCounter + 100)) {
      isrCounter += 10; // += 10 because one tick = 10ms
    }
  }
#else
  void IRAM_ATTR onTimer1ISR() {
    timer1_write(50000); // set interrupt time to 10ms
    if (isrCounter >= heaterOverextendingIsrCounter) {
      // turn off when when compute() is not run in time (safetly measure)
      digitalWrite(pinRelayHeater, LOW);
      // ERROR_print("onTimer1ISR has stopped heater because pid.Compute() did not
      // run\n");
      // TODO: add more emergency handling?
    } else if (isrCounter > windowSize) {
      // dont change output when overextending withing overextending_factor
      // threshold DEBUG_print("onTimer1ISR over extending due to processing
      // delays: isrCounter=%u\n", isrCounter);
    } else if (isrCounter >= Output) { // max(Output) = windowSize
      digitalWrite(pinRelayHeater, LOW);
    } else {
      digitalWrite(pinRelayHeater, HIGH);
    }
    if (isrCounter <= (heaterOverextendingIsrCounter + 100)) {
      isrCounter += 10; // += 10 because one tick = 10ms
    }
  }
#endif


// Check mqtt connection
void CheckMqttConnection() {
  if (MQTT_ENABLE && !mqttDisabledTemporary) {
    if (!isMqttWorking()) {
      mqttReconnect(false);
    } else {
      unsigned long now = millis();
      if (now >= lastMQTTStatusReportTime + lastMQTTStatusReportInterval) {
        lastMQTTStatusReportTime = now;
        mqttPublish((char*)"temperature", number2string(Input));
        mqttPublish((char*)"temperatureAboveTarget", number2string((Input - *activeSetPoint)));
        mqttPublish((char*)"heaterUtilization", number2string(convertOutputToUtilisation(Output)));
        mqttPublish((char*)"pastTemperatureChange", number2string(tempSensor.pastTemperatureChange(10*10)));
        mqttPublish((char*)"brewReady", bool2string(brewReady));
        if (ENABLE_POWER_OFF_COUNTDOWN != 0) {
          powerOffTimer = ENABLE_POWER_OFF_COUNTDOWN - ((millis() - lastBrewEnd) / 1000);
          mqttPublish((char*)"powerOffTimer", int2string(powerOffTimer >= 0 ? ((powerOffTimer + 59) / 60) : 0)); // in minutes always rounded up
        }
        // mqttPublishSettings();  //not needed because we update live on occurence
      }
#if (MQTT_ENABLE == 1)
      if (millis() >= previousTimerMqttHandle + 100) {
        previousTimerMqttHandle = millis();
        mqttClient.loop(); // mqtt client connected, do mqtt housekeeping
      }
#endif
    }
  }
}

  /***********************************
   * LOOP()
   ***********************************/
  void loop() {
    Input = tempSensor.refresh(Input, activeState, activeSetPoint, &secondlatestTemperature); // save new temperature values
    
    if (tempSensor.malfunction) {
      snprintf(debugLine, sizeof(debugLine), "temp sensor malfunction: latestTemperature=%0.2f, secondlatestTemperature=%0.2f", tempSensor.getLatestTemperature(), secondlatestTemperature);
      ERROR_println(debugLine);
      mqttPublish((char*)"events", debugLine);
    }

    testEmergencyStop(); // test if Temp is to high

    set_profile();

    // brewReady
    if (millis() > lastCheckBrewReady + 1000) {
      lastCheckBrewReady = millis();
      bool brewReadyCurrent = checkBrewReady(*activeSetPoint, marginOfFluctuation, 60*10);
      if (!brewReady && brewReadyCurrent) {
        snprintf(debugLine, sizeof(debugLine), "brewReady (Tuning took %lu secs)", ((lastCheckBrewReady - lastBrewEnd) / 1000) - 60);
        DEBUG_println(debugLine);
        mqttPublish((char*)"events", debugLine);
        lastBrewReady = millis() - 60000;
      } else if (brewReady && !brewReadyCurrent) {
        //DEBUG_print("brewReady off: %d = %0.2f, %0.2f, %0.2f, %0.2f, %0.2f = %0.2f\n", readIndex, getTemperature(0), getTemperature(1), getTemperature(2), getTemperature(3), getTemperature(4), getAverageTemperature(5));
      }
      brewReady = brewReadyCurrent;
    }
    setHardwareLed(((brewReady && (ENABLE_HARDWARE_LED_OFF_WHEN_SCREENSAVER == 0 || screenSaverOn == false))) || (steaming && Input >= steamReadyTemp));

    // network related stuff
    if (!forceOffline) {
      if (!isWifiWorking()) {
#if (MQTT_ENABLE == 2)
        MQTT_server_cleanupClientCons();
#endif
        checkWifi(inSensitivePhase());
      } else {
        HandleOTA();
        runBlynk();
        CheckMqttConnection();
      }
    }

    if (millis() >= previousTimerDebugHandle + 200) {
      previousTimerDebugHandle = millis();
      Debug.handle();
    }

#if (SCALE_SENSOR_ENABLE)
    updateWeight(); // get new weight values
#endif

#if (1 == 0)
    performance_check();
    return;
#endif

#if (ENABLE_CALIBRATION_MODE == 1)
    if (millis() > lastCheckGpio + 2500) {
      lastCheckGpio = millis();
      debugControlHardware(controlsConfig);
      debugWaterLevelSensor();
      #if (SCALE_SENSOR_ENABLE)
      scaleCalibration();
      #endif
      displaymessage(State::Undefined, (char*)"Calibrating", (char*)"check logs");
    }
    return;
#endif

    pidCompute(); // call PID for Output calculation

    checkControls(controlsConfig); // transform controls to actions

    if (activeState == State::CleanMode) {
      clean();
    } else {
      // handle brewing if button is pressed (ONLYPID=0 for now, because
      // ONLYPID=1_with_BREWDETECTION=1 is handled in actionControl).
      // ideally brew() should be controlled in our state-maschine (TODO)
      brew();
    }

    // check if PID should run or not. If not, set to manuel and force output to zero
    if (millis() > previousTimerPidCheck + 300) {
      previousTimerPidCheck = millis();
      if (pidON == 0 && pidMode == 1) {
        pidMode = 0;
        bPID.SetMode(pidMode);
        Output = 0;
        DEBUG_print("Current config has disabled PID\n");
      } else if (pidON == 1 && pidMode == 0 && !emergencyStop) {
        Output = 0; // safety: be 100% sure that PID.compute() starts fresh.
        pidMode = 1;
        bPID.SetMode(pidMode);
        if (millis() - recurringOutput > 21000) {
          DEBUG_print("Current config has enabled PID\n");
          recurringOutput = millis();
        }
      }
    }

    // update sleep status
    if (millis() > previousTimerSleepCheck + 300) {
      if (sleeping) {
        if (userActivity != userActivitySavedOnForcedSleeping) { actionController(SLEEPING, 0, true); }
      } else if (heaterInactivityTimer > 0) {
        if (millis() > userActivity + heaterInactivityTimer) { actionController(SLEEPING, 1, true); }
      }
    }

    // powerDown scale + automatic calculation of scaleSensorWeightOffset + output brew statistics x seconds after brew happened
    #if (SCALE_SENSOR_ENABLE)
    if (!brewing && (millis() > brewStatisticsTimer + brewStatisticsAdditionalWeightTime) ) {
        if (scaleRunning && (*activeBrewTimeEndDetection >= 1) && scaleSensorWeightOffsetAtStop != 0) {
          if (*activeBrewTimeEndDetection >= 1 && getTareAsyncStatus() && (brewTimer <= (totalBrewTime + (brewtimeMaxAdditionalTimeWhenWeightNotReached * 1000))) ) {
            float scaleSensorWeightOffsetCurrent = (*activeScaleSensorWeightSetPoint - currentWeight) * -1;
            float scaleSensorWeightOffsetCurrentRelative = (*activeScaleSensorWeightSetPoint - currentWeight - scaleSensorWeightOffset) * -1;

            if (abs(scaleSensorWeightOffsetCurrent) >= 0.05 && abs(scaleSensorWeightOffsetCurrent) <= scaleSensorWeightOffsetMax) {
              float scaleSensorWeightOffsetPrev = scaleSensorWeightOffset;
              scaleSensorWeightOffset = (scaleSensorWeightOffsetPrev * (1-scaleSensorWeightOffsetFactor)) + scaleSensorWeightOffsetCurrentRelative * scaleSensorWeightOffsetFactor;

              if (scaleSensorWeightOffset >= scaleSensorWeightOffsetMax) scaleSensorWeightOffset = scaleSensorWeightOffsetMax;
              else if (scaleSensorWeightOffset <= scaleSensorWeightOffsetMin) scaleSensorWeightOffset = scaleSensorWeightOffsetMin;

              DEBUG_print("Auto-tune scaleSensorWeightOffset=%0.2fg (OffsetPrev=%0.2fg diff=%0.2fg)\n", scaleSensorWeightOffset, scaleSensorWeightOffsetPrev, scaleSensorWeightOffsetCurrentRelative);
              eepromForceSync = millis();
            }
          }
          snprintf(debugLine, sizeof(debugLine), "Brew statistics: %0.2fg in %0.2fs (%0.2fg/s) with profile %u", currentWeight, brewTimer/1000.0, 1000*currentWeight/brewTimer, profile);
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", (char*)debugLine);
          scalePowerDown(); 
        }
    }
    #endif

    // Sicherheitsabfrage
    if (!tempSensor.malfunction && !emergencyStop && Input > 0) {
      updateState();

      /* state 1: Water is very cold, set heater to full power */
      if (activeState == State::ColdStart) {
        Output = windowSize;

        /* state 2: ColdstartTemp reached. Now stabilizing temperature after
         * coldstart */
      } else if (activeState == State::StabilizeTemperature) {
        // Output = convertUtilisationToOutput(steadyPower +
        // bPID.GetSteadyPowerOffsetCalculated());
        Output = convertUtilisationToOutput(steadyPower);

        /* state 4: Brew detected. Increase heater power */
      } else if (activeState == State::BrewDetected) {
        if (Input > *activeSetPoint + outerZoneTemperatureDifference) {
          Output = convertUtilisationToOutput(steadyPower + bPID.GetSteadyPowerOffsetCalculated());
        } else {
          Output = convertUtilisationToOutput(brewDetectionPower);
        }
        if (OnlyPID == 1) {
          if (timerBrewDetection == 1) { brewTimer = millis() - lastBrewTime; }
        }

        /* state 5: Outer Zone reached. More power than in inner zone */
      } else if (activeState == State::OuterZoneDetected) {
        if (Input > *activeSetPoint) {
          Output = 0;
        } else {
          if (aggoTn != 0) {
            aggoKi = aggoKp / aggoTn;
          } else {
            aggoKi = 0;
          }
          aggoKd = aggoTv * aggoKp;
          bPID.SetTunings(aggoKp, aggoKi, aggoKd);
          if (pidMode == 1) bPID.SetMode(AUTOMATIC);
        }

        /* state 6: Steaming state active*/
      } else if (activeState == State::SteamMode) {
        bPID.SetMode(MANUAL);
        if (!pidMode) {
          if (millis() >= streamComputeLastRunTime + 500) {
            ERROR_print("steam: must not be inside this\n");
            streamComputeLastRunTime = millis();
          }
          Output = 0;
        } else {
          if (millis() >= streamComputeLastRunTime + 1000) {
            streamComputeLastRunTime = millis();
            
            if (Input <= setPointSteam) {
              // full heat when temp below steam-temp
              //DEBUG_print("steam: input=%0.2f, past2s=%0.2f HEATING\n", Input,  tempSensor.pastTemperatureChange(2*10));
              Output = windowSize;
            } else if (Input > setPointSteam && (tempSensor.pastTemperatureChange(2*10) < -0.05)) {
              // full heat when >setPointSteam BUT temp goes down!
              //DEBUG_print("steam: input=%0.2f, past2s=%0.2f HEATING ABOVE\n", Input,  tempSensor.pastTemperatureChange(2*10));
              Output = windowSize;
            } else {
              //DEBUG_print("steam: input=%0.2f, past2s=%0.2f\n", Input,  tempSensor.pastTemperatureChange(2*10));
              Output = 0;
            }
          }
        }

        /* state 7: Sleeping state active*/
      } else if (activeState == State::SleepMode) {
        if (millis() - recurringOutput > 60000) {
          recurringOutput = millis();
          snprintf(debugLine, sizeof(debugLine), "sleeping...");
          DEBUG_println(debugLine);
        }
        Output = 0;

        /* state 8: Cleaning state active*/
      } else if (activeState == State::CleanMode) {
        if (millis() - recurringOutput > 60000) {
          recurringOutput = millis();
          snprintf(debugLine, sizeof(debugLine), "cleaning...");
          DEBUG_println(debugLine);
        }
        if (!pidMode) {
          Output = 0;
        } else {
          bPID.SetMode(AUTOMATIC);
          if (aggTn != 0) {
            aggKi = aggKp / aggTn;
          } else {
            aggKi = 0;
          }
          aggKd = aggTv * aggKp;
          bPID.SetTunings(aggKp, aggKi, aggKd);
        }

        /* state 3: Inner zone reached = "normal" low power mode */
      } else {
        if (!pidMode) {
          Output = 0;
        } else {
          bPID.SetMode(AUTOMATIC);
          if (aggTn != 0) {
            aggKi = aggKp / aggTn;
          } else {
            aggKi = 0;
          }
          aggKd = aggTv * aggKp;
          bPID.SetTunings(aggKp, aggKi, aggKd);
        }
      }

      maintenance(); // update displayMessageLine1 & Line2
      displaymessage(activeState, (char*)displayMessageLine1, (char*)displayMessageLine2);

      sendToBlynk();
      
#if (1 == 0)
      performance_check();
      return;
#endif

    } else if (tempSensor.malfunction) {
      // Deactivate PID
      if (pidMode == 1) {
        pidMode = 0;
        bPID.SetMode(pidMode);
        Output = 0;
        if (millis() - recurringOutput > 15000) {
          ERROR_print("sensor malfunction detected. Shutdown PID and heater\n");
          recurringOutput = millis();
        }
      }
      digitalWrite(pinRelayHeater, LOW); // Stop heating
      char line2[17];
      snprintf(line2, sizeof(line2), "Temp. %0.2f", tempSensor.getCurrentTemperature());
      displaymessage(State::Undefined, (char*)"Check Temp. Sensor!", (char*)line2);

    } else if (emergencyStop) {
      // Deactivate PID
      if (pidMode == 1) {
        pidMode = 0;
        bPID.SetMode(pidMode);
        Output = 0;
        if (millis() - recurringOutput > 10000) {
          ERROR_print("emergencyStop detected. Shutdown PID and heater (temp=%0.2f)\n", tempSensor.getCurrentTemperature());
          recurringOutput = millis();
        }
      }
      digitalWrite(pinRelayHeater, LOW); // Stop heating
      char line2[17];
      snprintf(line2, sizeof(line2),
          "%0.0f\xB0"
          "C",
          tempSensor.getCurrentTemperature());
      displaymessage(State::Undefined, (char*)"Emergency Stop!", (char*)line2);

    } else {
      if (millis() - recurringOutput > 15000) {
        ERROR_print("unknown error\n");
        recurringOutput = millis();
      }
    }

    // persist steadyPower auto-tuning setting
    #if (MQTT_ENABLE == 1)
    if (!almostEqual(steadyPower, steadyPowerSaved) && steadyPowerMQTTDisableUpdateUntilProcessed == 0) { // prevent race conditions by semaphore
      steadyPowerSaved = steadyPower;
      steadyPowerMQTTDisableUpdateUntilProcessed = steadyPower;
      steadyPowerMQTTDisableUpdateUntilProcessedTime = millis();
      mqttPublish((char*)"steadyPower/set", number2string(steadyPower)); // persist value over shutdown
      mqttPublish((char*)"steadyPower", number2string(steadyPower));
      if (eepromForceSync == 0) {
        eepromForceSync = millis() + 600000; // reduce writes on eeprom
      }
    }
    if ((steadyPowerMQTTDisableUpdateUntilProcessedTime > 0) && (millis() >= steadyPowerMQTTDisableUpdateUntilProcessedTime + 20000)) {
      ERROR_print("steadyPower setting not saved for over 20sec "
                  "(steadyPowerMQTTDisableUpdateUntilProcessed=%0.2f)\n", steadyPowerMQTTDisableUpdateUntilProcessed);
      steadyPowerMQTTDisableUpdateUntilProcessedTime = 0;
      steadyPowerMQTTDisableUpdateUntilProcessed = 0;
    }
    #endif

    // persist settings to eeprom on interval or when required
    if (!inSensitivePhase() && (millis() >= eepromSaveTime + eepromSaveTimer || (eepromForceSync > 0 && (millis() >= eepromForceSync + eepromForceSyncWaitTimer)))) {
      eepromSaveTime = millis();
      eepromForceSync = 0;
      sync_eeprom();
    }
#ifdef ESP32
    yieldIfNecessary();  // it seems esp32 multicore needs some time to complete internal house keeping
#endif
  }

  /***********************************
   * WATER LEVEL SENSOR & MAINTENANCE
   ***********************************/
  void maintenance() {
    static int maintenanceOperation = 0;  // which maintenance operation is currently processed? the others shall not run
#if (WATER_LEVEL_SENSOR_ENABLE)
    if ((maintenanceOperation == 0 || maintenanceOperation == 1) && (millis() >= previousTimerWaterLevelCheck + waterSensorCheckTimer)) {
      previousTimerWaterLevelCheck = millis();
      unsigned int waterLevelMeasured = waterSensor.readRangeContinuousMillimeters();
      if (waterSensor.timeoutOccurred()) {
        ERROR_println("Water level sensor: TIMEOUT");
        snprintf(displayMessageLine1, sizeof(displayMessageLine1), "Water sensor defect");
        maintenanceOperation = 1;
      } else if (waterLevelMeasured >= WATER_LEVEL_SENSOR_LOW_THRESHOLD) {
        DEBUG_print("Water level is low: %u mm (low_threshold: %u)\n", waterLevelMeasured, WATER_LEVEL_SENSOR_LOW_THRESHOLD);
        snprintf(displayMessageLine1, sizeof(displayMessageLine1), "Water is low!");
        maintenanceOperation = 1;
      } else if (maintenanceOperation == 1) {
        displayMessageLine1[0] = '\0';
        maintenanceOperation = 0;
      }
    }
#endif
#if (SCALE_SENSOR_ENABLE)
    if ((maintenanceOperation == 0 || maintenanceOperation == 2) && 
       (millis() >= previousTimerScaleStatistics + scaleSensorCheckTimer) && 
       (millis() >= brewStatisticsTimer + brewStatisticsAdditionalWeightTime) &&
       (millis() <= brewStatisticsTimer + brewStatisticsAdditionalDisplayTime * (2+2))
    ) {
      previousTimerScaleStatistics = millis();
      if ( (brewTimer > 0) && (currentWeight != 0) && 
          (millis() >= brewStatisticsTimer + brewStatisticsAdditionalWeightTime) &&
          (millis() <= brewStatisticsTimer + brewStatisticsAdditionalDisplayTime * 2) ) {
            maintenanceOperation = 2;
            snprintf(displayMessageLine1, sizeof(displayMessageLine1), "%0.2fg in %0.2fs", currentWeight, brewTimer/1000.0);
            snprintf(displayMessageLine2, sizeof(displayMessageLine2), "%0.2fg/s", 1000*currentWeight/brewTimer);
      } else if (maintenanceOperation == 2) {
        displayMessageLine1[0] = '\0';
        displayMessageLine2[0] = '\0';
        maintenanceOperation = 0;
      } 
    }
#endif
  }

  void debugWaterLevelSensor() {
#if (WATER_LEVEL_SENSOR_ENABLE)
    unsigned long start = millis();
    unsigned int waterLevelMeasured = waterSensor.readRangeContinuousMillimeters();
    if (waterSensor.timeoutOccurred()) {
      ERROR_println("WATER_LEVEL_SENSOR: TIMEOUT");
    } else
      DEBUG_print("WATER_LEVEL_SENSOR: %u mm (low_threshold: %u) (took: %lu ms)\n", waterLevelMeasured, WATER_LEVEL_SENSOR_LOW_THRESHOLD, millis() - start);
#endif
  }

  void performance_check() {  // has an influence on esp housekeeping
    static int printCounter = 0;
    loops += 1;
    curMicros = micros();
    if (maxMicros < curMicros - curMicrosPreviousLoop) { maxMicros = curMicros - curMicrosPreviousLoop; }
    if (curMicros >= lastReportMicros + 100000) { // 100ms
      printCounter++;
      if (true || printCounter >20) {
        DEBUG_print("%lu loop() | loops/ms=%lu | spend_micros_last_loop=%lu | "
          "max_micros_since_last_report=%lu | avg_micros/loop=%lu\n",
          curMicros / 1000, loops / 100, (curMicros - curMicrosPreviousLoop), maxMicros, (curMicros - lastReportMicros) / loops);
        printCounter = 0;
      }
      lastReportMicros = curMicros;
      maxMicros = 0;
      loops = 0;
    }
    curMicrosPreviousLoop = curMicros;
  }

  void set_profile() {
    set_profile(false);
  }

  void set_profile(bool force) {
    if (!force && ((profile == activeProfile || steaming))) return;
    if (profile >=4) { profile = 3; }
    else if (profile <= 0) { profile = 1; }
    DEBUG_print("Activating profile %d\n", profile);
    switch (profile) {
      case 2:
        activeSetPoint = &setPoint2;
        activeStartTemp = &starttemp2;
        activeBrewTime = &brewtime2;
        activePreinfusion = &preinfusion2;
        activePreinfusionPause = &preinfusionpause2;
        activeBrewTimeEndDetection = &brewtimeEndDetection2;
        activeScaleSensorWeightSetPoint = &scaleSensorWeightSetPoint2;
        activeProfile = profile;
        break;
      case 3:
        activeSetPoint = &setPoint3;
        activeStartTemp = &starttemp3;
        activeBrewTime = &brewtime3;
        activePreinfusion = &preinfusion3;
        activePreinfusionPause = &preinfusionpause3;
        activeBrewTimeEndDetection = &brewtimeEndDetection3;
        activeScaleSensorWeightSetPoint = &scaleSensorWeightSetPoint3;
        activeProfile = profile;
        break;
      default:
        activeSetPoint = &setPoint1;
        activeStartTemp = &starttemp1;
        activeBrewTime = &brewtime1;
        activePreinfusion = &preinfusion1;
        activePreinfusionPause = &preinfusionpause1;
        activeBrewTimeEndDetection = &brewtimeEndDetection1;
        activeScaleSensorWeightSetPoint = &scaleSensorWeightSetPoint1;
        activeProfile = profile = 1;
        break;
    }
    mqttPublish((char*)"profile/set", number2string(activeProfile));
    mqttPublish((char*)"activeBrewTime/set", number2string(*activeBrewTime));
    mqttPublish((char*)"activeStartTemp/set", number2string(*activeStartTemp));
    mqttPublish((char*)"activeSetPoint/set", number2string(*activeSetPoint));
    mqttPublish((char*)"activePreinfusion/set", number2string(*activePreinfusion));
    mqttPublish((char*)"activePreinfusionPause/set", number2string(*activePreinfusionPause));
    mqttPublish((char*)"activeBrewTimeEndDetection/set", number2string(*activeBrewTimeEndDetection));
    mqttPublish((char*)"activeScaleSensorWeightSetPoint/set", number2string(*activeScaleSensorWeightSetPoint));
    mqttPublish((char*)"profile", number2string(activeProfile));
    mqttPublish((char*)"activeBrewTime", number2string(*activeBrewTime));
    mqttPublish((char*)"activeStartTemp", number2string(*activeStartTemp));
    mqttPublish((char*)"activeSetPoint", number2string(*activeSetPoint));
    mqttPublish((char*)"activePreinfusion", number2string(*activePreinfusion));
    mqttPublish((char*)"activePreinfusionPause", number2string(*activePreinfusionPause));
    mqttPublish((char*)"activeBrewTimeEndDetection", number2string(*activeBrewTimeEndDetection));
    mqttPublish((char*)"activeScaleSensorWeightSetPoint", number2string(*activeScaleSensorWeightSetPoint));
    blynkSave((char*)"profile");
    blynkSave((char*)"activeBrewTime");
    blynkSave((char*)"activeStartTemp");
    blynkSave((char*)"activeSetPoint");
    blynkSave((char*)"activePreinfusion");
    blynkSave((char*)"activePreinfusionPause");
    blynkSave((char*)"activeBrewTimeEndDetection");
    blynkSave((char*)"activeScaleSensorWeightSetPoint");
  }

  void print_settings() {
    DEBUG_print("========================\n");
    DEBUG_print("Machine: %s | Version: %s\n", MACHINE_TYPE, sysVersion);
    DEBUG_print("aggKp: %0.2f | aggTn: %0.2f | aggTv: %0.2f\n", aggKp, aggTn, aggTv);
    DEBUG_print("aggoKp: %0.2f | aggoTn: %0.2f | aggoTv: %0.2f\n", aggoKp, aggoTn, aggoTv);
    DEBUG_print("profile: %u | starttemp: %0.2f \n", profile, *activeStartTemp);
    DEBUG_print("setPointSteam: %0.2f | activeSetPoint: %0.2f\n", setPointSteam, *activeSetPoint);
    DEBUG_print("brewDetection: %d | brewDetectionSensitivity: %0.2f | brewDetectionPower: %0.2f\n",
        brewDetection, brewDetectionSensitivity, brewDetectionPower);
    DEBUG_print("activeBrewTime: %0.2f | activePreinfusion: %0.2f | activePreinfusionPause: %0.2f\n", *activeBrewTime, *activePreinfusion, *activePreinfusionPause);
    DEBUG_print("activeBrewTimeEndDetection: %d | activeScaleSensorWeightSetPoint: %0.2f\n", *activeBrewTimeEndDetection, *activeScaleSensorWeightSetPoint);
    DEBUG_print("cleaningCycles: %d | cleaningInterval: %d | cleaningPause: %d\n", cleaningCycles, cleaningInterval, cleaningPause);
    DEBUG_print("steadyPower: %0.2f | steadyPowerOffset: %0.2f | steadyPowerOffsetTime: %u\n",
        steadyPower, steadyPowerOffset, steadyPowerOffsetTime);
    DEBUG_print("pidON: %d | tempSensor: %s\n", pidON, tempSensor.name);
    printControlsConfig(controlsConfig);
    printMultiToggleConfig();
    printMenuConfig(menuConfig);
    DEBUG_print("========================\n");
  }

  void InitDebug()
  {
    DEBUGSTART(115200);

    Debug.begin(hostname, Debug.DEBUG);
    Debug.setResetCmdEnabled(true); // Enable the reset command
    Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
    Debug.showColors(true); // Colors
    Debug.setSerialEnabled(true); // log to Serial also
    // Serial.setDebugOutput(true); // enable diagnostic output of WiFi
    // libraries
    Debug.setCallBackNewClient(&print_settings);  

    DEBUG_print("\nMachine: %s\nVersion: %s\n", MACHINE_TYPE, sysVersion);
  }

  /********************************************************
   * Init Pins
  ******************************************************/
  void InitPins() {
    pinMode(pinRelayValve, OUTPUT);
    digitalWrite(pinRelayValve, valveRelayOFF);
    pinMode(pinRelayPump, OUTPUT);
    digitalWrite(pinRelayPump, pumpRelayOFF);
    pinMode(pinRelayHeater, OUTPUT);
    digitalWrite(pinRelayHeater, LOW);
#if (ENABLE_HARDWARE_LED == 1)
    pinMode(pinLed, OUTPUT);
    digitalWrite(pinLed, LOW);
    setHardwareLed(1);
#elif (ENABLE_HARDWARE_LED == 2)
    pinMode(pinLed, OUTPUT);
    FastLED.addLeds<WS2812B, pinLed, GRB>(leds, enabledHardwareLedNumber);
    setHardwareLed(1);
#endif

#if (ENABLE_GPIO_ACTION == 1)
  #ifdef pinBrewAction
    pinMode(pinBrewAction, OUTPUT);
    setGpioAction(BREWING, 1);
  #endif
  #ifdef pinHotwaterAction
    pinMode(pinHotwaterAction, OUTPUT);
    setGpioAction(HOTWATER, 1);
  #endif
  #ifdef pinSteamingAction
    pinMode(pinSteamingAction, OUTPUT);
    setGpioAction(STEAMING, 1);
  #endif
#endif
  }

  /********************************************************
  * Define trigger type
  ******************************************************/
  void DefineTriggerTypes() {
    valveRelayON = valveTriggerType ? HIGH: LOW;
    valveRelayOFF = !valveRelayON;
    pumpRelayON = pumpTriggerType ? HIGH: LOW;
    pumpRelayOFF = !pumpRelayON;
  }

  /********************************************************
  * Init PID
  ******************************************************/
  void InitPid() {
    bPID.SetSampleTime(windowSize);
    bPID.SetOutputLimits(0, windowSize);
    bPID.SetMode(AUTOMATIC);
  }

  /********************************************************
  * INIT SCALE
  ******************************************************/
  void InitScale() {
#if (SCALE_SENSOR_ENABLE)
    initScale();
    scalePowerDown();
#endif
  }

 /********************************************************
 * WATER LEVEL SENSOR
 ******************************************************/
void InitWaterLevelSensor() {
	#if (WATER_LEVEL_SENSOR_ENABLE)
	#ifdef ESP32
		Wire1.begin(WATER_LEVEL_SENSOR_SDA, WATER_LEVEL_SENSOR_SCL,
			100000U); // Wire0 cannot be re-used due to core0 stickyness
		waterSensor.setBus(&Wire1);
	#else
		Wire.begin();
		waterSensor.setBus(&Wire);
	#endif
		waterSensor.setTimeout(300);
		if (!waterSensor.init()) {
		  ERROR_println("Water level sensor cannot be initialized");
		  displaymessage(State::Undefined, (char*)"Water sensor defect", (char*)"");
		}
		// increased accuracy by increase timing budget to 200 ms
		waterSensor.setMeasurementTimingBudget(200000);
		// continuous timed mode
		waterSensor.startContinuous(ENABLE_CALIBRATION_MODE == 1 ? 1000 : waterSensorCheckTimer / 2);
	#endif
}

/********************************************************
 * READ/SAVE EEPROM
 * get latest values from EEPROM if not already fetched from blynk or
 * remote mqtt-server. Additionally this function honors changed values in
 * userConfig.h (changed userConfig.h values have priority). Some special
 * variables like profile-dependent ones are always fetched from eeprom.
 ******************************************************/
void InititialSyncEeprom(bool force_read) {	 
    #ifndef ESP32
    EEPROM.begin(432);
    #endif

    sync_eeprom(true, force_read);
}

/********************************************************
 * PUBLISH settings on MQTT (and wait for them to be processed!)
 * + SAVE settings on MQTT-server if MQTT_ENABLE==1
 ******************************************************/
void InitMqttPublishSettings() {
    steadyPowerSaved = steadyPower;
    if (isMqttWorking()) {
      steadyPowerMQTTDisableUpdateUntilProcessed = steadyPower;
      steadyPowerMQTTDisableUpdateUntilProcessedTime = millis();
      mqttPublishSettings();
#if (MQTT_ENABLE == 1)
      unsigned long started = millis();
      while ((millis() < started + 4000) && (steadyPowerMQTTDisableUpdateUntilProcessed != 0)) {
        mqttClient.loop();
      }
      if (steadyPowerMQTTDisableUpdateUntilProcessed != 0) {
        ERROR_print("InitialMqttPublishSettings() was not able to process all mqtt values in time.\n");
      }
#endif
    }
}	




/********************************************************
 * TEMP SENSOR
 ******************************************************/
void InitTemperaturSensor() {
    isrCounter = 950; // required
    tempSensor.init();

    while (true) {
      secondlatestTemperature = tempSensor.read();
      Input = tempSensor.readWithDelay();
      if (tempSensor.checkSensor(activeState, activeSetPoint, &secondlatestTemperature) == SensorStatus::Ok) {
        tempSensor.updateTemperatureHistory(secondlatestTemperature);
        secondlatestTemperature = Input;
        DEBUG_print("Temp sensor check ok. Sensor init done\n");
        break;
      }
      displaymessage(State::Undefined, (char*)"Temp sensor defect", (char*)"");
      ERROR_print("Temp sensor defect. Cannot read consistent values. Retrying\n");
      HandleOTA();
      delay(1000);
    }
}

/********************************************************
 * Timer1 ISR - Initialisierung
 * TIM_DIV1 = 0,   //80MHz (80 ticks/us - 104857.588 us max)
 * TIM_DIV16 = 1,  //5MHz (5 ticks/us - 1677721.4 us max)
 * TIM_DIV256 = 3  //312.5Khz (1 tick = 3.2us - 26843542.4 us max)
 ******************************************************/
void InitTimer() {
	  isrCounter = 0;
	#ifdef ESP32
	  timer = timerBegin(0, 80, true); // 1Mhz
	  timerAttachInterrupt(timer, &onTimer1ISR, true);
	  timerAlarmWrite(timer, 10000, true); // 10ms
	  timerAlarmEnable(timer);
	#else
	  timer1_isr_init();
	  timer1_attachInterrupt(onTimer1ISR);
	  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
	  timer1_write(50000); // set interrupt time to 10ms
	#endif
}

/********************************************************
* CODEBLOCK for OTA begin
******************************************************/
void DisableTimerAlarm() {
#ifdef ESP32
  timerAlarmDisable(timer);
#else
  timer1_disable();
#endif
}

void EnableTimerAlarm() {
#ifdef ESP32
  timerAlarmEnable(timer);
#else
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
#endif
}

// OTA
unsigned long previousTimerOtaHandle = 0;
const bool ota = OTA;
const char* OTApass = OTAPASS;

void HandleOTA() {
	if (millis() >= previousTimerOtaHandle + 500) {
	  previousTimerOtaHandle = millis();
	  ArduinoOTA.handle();
	}
}

void InitOTA() {  
  if (ota && !forceOffline) {
    // TODO: OTA logic has to be refactored so have clean setup() and loop() parts
    // wifi connection is done during blynk connection
    ArduinoOTA.setHostname(hostname); //  Device name for OTA
    ArduinoOTA.setPassword(OTApass); //  Password for OTA
    ArduinoOTA.setRebootOnSuccess(true); // reboot after successful update 

	  // Disable interrupt when OTA starts, otherwise it will not work
	  ArduinoOTA.onStart([]() {
		  DEBUG_print("OTA update initiated\n");
		  Output = 0;
		  DisableTimerAlarm();
		  digitalWrite(pinRelayHeater, LOW); // Stop heating
		  activeState = State::SoftwareUpdate;
	  });
	  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		  int percent = progress / (total / 100);
		  DEBUG_print("OTA update in progress: %u%%\r", percent);
		  char line2[17];
		  snprintf(line2, sizeof(line2), "%u%% / 100%%", percent);
		  displaymessage(State::Undefined, (char*)"Updating Software", (char*)line2);
	  });    
	  ArduinoOTA.onError([](ota_error_t error) {
		  ERROR_print("OTA update error\n");
		  EnableTimerAlarm();
	  });
	  // Enable interrupts if OTA is finished
	  ArduinoOTA.onEnd([]() {
  		EnableTimerAlarm();
	  });
	  ArduinoOTA.begin();
  }
}
/********************************************************
* CODEBLOCK for OTA end
******************************************************/


/***********************************
 * SETUP()
 ***********************************/
void setup() {
  bool eeprom_force_read = true;

 #ifdef ESP32
  WiFi.useStaticBuffers(true);
  // required for remoteDebug to work
  WiFi.mode(WIFI_STA);
#endif

  InitDebug();

  DefineTriggerTypes();
  InitPins();

#if defined(OVERWRITE_VERSION_DISPLAY_TEXT)
  displaymessage(State::Undefined, (char*)DISPLAY_TEXT, (char*)OVERWRITE_VERSION_DISPLAY_TEXT);
#else
  displaymessage(State::Undefined, (char*)DISPLAY_TEXT, (char*)sysVersion);
#endif
  delay(1000);

  controlsConfig = parseControlsConfig();
  configureControlsHardware(controlsConfig);
  checkControls(controlsConfig);  // to get switch states at startup

  menuConfig = parseMenuConfig();
  
  // if simulatedBrewSwitch is already "on" on startup, then brew should
  // not start automatically
  if (simulatedBrewSwitch) {
    ERROR_print("Brewswitch is already turned on after power on. Don't brew until it is turned off.\n");
    waitingForBrewSwitchOff = true;
  }

  InitPid();
  InitScale();
  InitWifi(eeprom_force_read);
  InititialSyncEeprom(eeprom_force_read);

  set_profile();

  print_settings();

  InitMqttPublishSettings();
  InitOTA();  
  InitTemperaturSensor();
  InitWaterLevelSensor();

  /********************************************************
   * REST INIT()
   ******************************************************/
  setHardwareLed(0);
  setGpioAction(BREWING, 0);
  setGpioAction(STEAMING, 0);
  setGpioAction(HOTWATER, 0);

  InitTimer();

  // This initialisation MUST be at the very end of the setup(), otherwise the time
  // comparison in loop() will have a big offset
  unsigned long currentTime = millis();
  tempSensor.setPreviousTimerRefresh(currentTime);
  setPreviousTimerBlynk(currentTime + 800);
  lastMQTTStatusReportTime = currentTime + 300;
  pidComputeLastRunTime = currentTime;

  DEBUG_print("End of setup()\n");
}
