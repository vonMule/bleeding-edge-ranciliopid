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
#include "display.h"
#include "eeprom-pcpid.h"

RemoteDebug Debug;

const char* sysVersion PROGMEM = "3.2.0 beta 3";

/********************************************************
 * definitions below must be changed in the userConfig.h file
 ******************************************************/
const int OnlyPID = ONLYPID;
const int TempSensorRecovery = TEMPSENSORRECOVERY;
const int brewDetection = BREWDETECTION;
const int triggerType = TRIGGERTYPE;
const bool ota = OTA;
const int grafana = GRAFANA;

// Wifi
const char* hostname = HOSTNAME;
const char* ssid = D_SSID;
const char* pass = PASS;

unsigned long lastWifiConnectionAttempt = millis();
const unsigned long wifiReconnectInterval = 60000; // try to reconnect every 60 seconds
unsigned long wifiConnectWaitTime = 6000; // ms to wait for the connection to succeed
unsigned int wifiReconnects = 0; // number of reconnects

// OTA
const char* OTApass = OTAPASS;

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

WiFiClient espClient;

// MQTT
#if (MQTT_ENABLE == 1)
#include <PubSubClient.h>
PubSubClient mqttClient(espClient);
#elif (MQTT_ENABLE == 2)
#include <uMQTTBroker.h>
#endif
const int MQTT_MAX_PUBLISH_SIZE = 120; // see
                                       // https://github.com/knolleary/pubsubclient/blob/master/src/PubSubClient.cpp
const char* mqttServerIP = MQTT_SERVER_IP;
const int mqttServerPort = MQTT_SERVER_PORT;
const char* mqttUsername = MQTT_USERNAME;
const char* mqttPassword = MQTT_PASSWORD;
const char* mqttTopicPrefix = MQTT_TOPIC_PREFIX;
char topicWill[256];
char topicSet[256];
char topicActions[256];
unsigned long lastMQTTStatusReportTime = 0;
unsigned long lastMQTTStatusReportInterval = 5000; // mqtt send status-report every 5 second
const bool mqttFlagRetained = true;
unsigned long mqttDontPublishUntilTime = 0;
unsigned long mqttDontPublishBackoffTime = 60000; // Failsafe: dont publish if there are errors for 10 seconds
unsigned long mqttLastReconnectAttemptTime = 0;
unsigned int mqttReconnectAttempts = 0;
unsigned long mqttReconnectIncrementalBackoff = 210000; // Failsafe: add 210sec to reconnect time after each
                                                        // connect-failure.
unsigned int mqttMaxIncrementalBackoff = 5; // At most backoff <mqtt_max_incremenatl_backoff>+1 *
                                            // (<mqttReconnectIncrementalBackoff>ms)
bool mqttDisabledTemporary = false;
unsigned long mqttConnectTime = 0; // time of last successfull mqtt connection

/********************************************************
 * states
 ******************************************************/
int activeState = 3; // (0:= undefined / EMERGENCY_TEMP reached)
                     // 1:= Coldstart required (machine is cold)
                     // 2:= Stabilize temperature after coldstart
                     // 3:= (default) Inner Zone detected (temperature near setPoint)
                     // 4:= Brew detected 
                     // 5:= Outer Zone detected (temperature outside of "inner zone")
                     // 6:= steam mode activated
                     // 7:= sleep mode activated 
                     // 8:= clean mode

/********************************************************
 * history of temperatures
 *****************************************************/
const int numReadings = 75 * 10; // number of values per Array
float readingsTemp[numReadings]; // the readings from Temp
float readingsTime[numReadings]; // the readings from time
int readIndex = 0; // the index of the current reading
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
float steadyPowerSavedInBlynk = 0;
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
 * Sensor check
 ******************************************************/
bool sensorMalfunction = false;
int error = 0;
int errorFarOff = 0;
const int maxErrorCounter = 10*10; // define maximum number of consecutive polls (of refreshTempInterval) to have errors

/********************************************************
 * Rest
 *****************************************************/
unsigned int profile = 1;  // profile should be set
unsigned int activeProfile = profile;  // profile set

bool emergencyStop = false; // protect system when temperature is too high or sensor defect
int pidON = 1; // 1 = control loop in closed loop
int relayON, relayOFF; // used for relay trigger type. Do not change!
char displayMessageLine1[21];
char displayMessageLine2[21];
unsigned long userActivity = 0;
unsigned long userActivitySavedOnForcedSleeping = 0;
unsigned long previousTimerRefreshTemp; // initialisation at the end of init()
unsigned long previousTimerOtaHandle = 0;
unsigned long previousTimerMqttHandle = 0;
unsigned long previousTimerBlynkHandle = 0;
unsigned long previousTimerDebugHandle = 0;
unsigned long previousTimerPidCheck = 0;
#if (TEMPSENSOR==3)
const long refreshTempInterval = 200; // How often to read the temperature sensor (must be >=180ms)
#else
const long refreshTempInterval = 100; // How often to read the temperature sensor
#endif
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
char* blynkReadyLedColor = (char*)"#000000";
unsigned long lastCheckBrewReady = 0;
unsigned long lastBrewReady = 0;
unsigned long lastBrewEnd = 0; // used to determime the time it takes to reach brewReady==true
unsigned int powerOffTimer = 0;
bool brewReady = false;
unsigned long eepromSaveTimer = 28 * 60 * 1000UL; // save every 28min
unsigned long eepromSaveTime = 0;
char debugLine[200];
unsigned long recurringOutput = 0;
unsigned long allServicesLastReconnectAttemptTime = 0;
unsigned long allservicesMinReconnectInterval = 160000; // 160sec minimum wait-time between service reconnections
bool forceOffline = FORCE_OFFLINE;
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
 * Temperature Sensor: TSIC 30x TEMP / Max6675
 ******************************************************/
#if (TEMPSENSOR == 3)
  #define TEMPSENSOR_NAME "MAX6675"
  #include <max6675.h>
  MAX6675 thermocouple(pinTemperatureCLK, pinTemperatureCS, pinTemperatureSO);
#else // TSIC306 default sensor
  #define TEMPSENSOR_NAME "TSIC306"
  #include <ZACwire.h>
#if (!defined(ZACWIRE_VERSION) || (defined(ZACWIRE_VERSION) && ZACWIRE_VERSION <= 133L))
#error ERROR ZACwire library version must be >= 1.3.4
#endif
    #ifdef ESP32
ZACwire<pinTemperature> TSIC(306, TEMPSENSOR_BITWINDOW, 0, true);
    #else
ZACwire<pinTemperature> TSIC(306, TEMPSENSOR_BITWINDOW, 0, true);
    #endif
#endif    

uint16_t temperature = 0;
volatile uint16_t temp_value[2] = { 0 };
volatile byte tsicDataAvailable = 0;
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
  ERROR_print("WiFi lost connection. (IP=%s) Reason: %u\n", WiFi.localIP().toString().c_str(), info.disconnected.reason);
}
#endif

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

#if (BLYNK_ENABLE==1)
#define BLYNK_PRINT Serial
#ifdef ESP32
#include <BlynkSimpleEsp32.h>
#else
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#endif

/******************************************************
 * Receive following BLYNK PIN values from app/server
 ******************************************************/
BLYNK_CONNECTED() {
  blynkConnectTime = millis();
  if (!blynkSyncRunOnce) {
    blynkSyncRunOnce = true;
    Blynk.syncAll(); // get all values from server/app when connected
  }
}
// This is called when Smartphone App is opened
BLYNK_APP_CONNECTED() {
  DEBUG_print("Blynk Client Connected.\n");
  print_settings();
  printControlsConfig(controlsConfig);
  printMenuConfig(menuConfig);
  // one time refresh on connect cause BLYNK_READ seems not to work always
  blynkSave((char*)"cleaningCycles");
  blynkSave((char*)"cleaningInterval");
  blynkSave((char*)"cleaningPause");
}
// This is called when Smartphone App is closed
BLYNK_APP_DISCONNECTED() { DEBUG_print("Blynk Client Disconnected.\n"); }
BLYNK_WRITE(V3) { profile = param.asInt(); }
BLYNK_WRITE(V4) { aggKp = param.asFloat(); }
BLYNK_WRITE(V5) { aggTn = param.asFloat(); }
BLYNK_WRITE(V6) { aggTv = param.asFloat(); }
BLYNK_WRITE(V7) { if ((millis() - blynkConnectTime > 10000 )) *activeSetPoint = param.asFloat(); }
BLYNK_WRITE(V8) { if ((millis() - blynkConnectTime > 10000 )) *activeBrewTime = param.asFloat(); }
BLYNK_WRITE(V9) { if ((millis() - blynkConnectTime > 10000 )) *activePreinfusion = param.asFloat(); }
BLYNK_WRITE(V10) { if ((millis() - blynkConnectTime > 10000 )) *activePreinfusionPause = param.asFloat(); }
BLYNK_WRITE(V12) { if ((millis() - blynkConnectTime > 10000 )) *activeStartTemp = param.asFloat(); }
BLYNK_WRITE(V13) { pidON = param.asInt() == 1 ? 1 : 0; }
BLYNK_WRITE(V30) { aggoKp = param.asFloat(); }
BLYNK_WRITE(V31) { aggoTn = param.asFloat(); }
BLYNK_WRITE(V32) { aggoTv = param.asFloat(); }
BLYNK_WRITE(V34) { brewDetectionSensitivity = param.asFloat(); }
BLYNK_WRITE(V36) { brewDetectionPower = param.asFloat(); }
//BLYNK_WRITE(V40) { burstShot = param.asInt(); }
BLYNK_WRITE(V41) {
  steadyPower = param.asFloat();
  // TODO fix this bPID.SetSteadyPowerDefault(steadyPower); //TOBIAS: working?
}
BLYNK_WRITE(V42) { steadyPowerOffset = param.asFloat(); }
BLYNK_WRITE(V43) { steadyPowerOffsetTime = param.asInt(); }
//BLYNK_WRITE(V44) { burstPower = param.asFloat(); }
BLYNK_WRITE(V50) { setPointSteam = param.asFloat(); }
BLYNK_READ(V51) { blynkSave((char*)"cleaningCycles"); }
BLYNK_READ(V52) { blynkSave((char*)"cleaningInterval"); }
BLYNK_READ(V53) { blynkSave((char*)"cleaningPause"); }
BLYNK_WRITE(V101) {
  int val = param.asInt();
  if (((millis() - blynkConnectTime < 10000 )) && val != 0) {
    actionController(BREWING, 0, true, false);
    Blynk.virtualWrite(V101, 0);
  } else {
    actionController(BREWING, val, true, false);
  }
}
BLYNK_WRITE(V102) {
  int val = param.asInt();
  if (((millis() - blynkConnectTime < 10000 )) && val != 0) {
    actionController(HOTWATER, 0, true, false);
    Blynk.virtualWrite(V102, 0);
  } else {
    actionController(HOTWATER, val, true, false);
  }
}
BLYNK_WRITE(V103) {
  int val = param.asInt();
  if (((millis() - blynkConnectTime < 10000 )) && val != 0) {
    actionController(STEAMING, 0, true, false);
    Blynk.virtualWrite(V103, 0);
  } else {
    actionController(STEAMING, val, true, false);
  }
}
BLYNK_WRITE(V107) {
  int val = param.asInt();
  if (((millis() - blynkConnectTime < 10000 )) && val != 0) {
    actionController(CLEANING, 0, true, false);
    Blynk.virtualWrite(V107, 0);
  } else {
    actionController(CLEANING, val, true, false);
  }
}
BLYNK_WRITE(V110) {
  int val = param.asInt();
  if (((millis() - blynkConnectTime < 10000 )) && val != 0) {
    actionController(SLEEPING, 0, true, false);
    Blynk.virtualWrite(V110, 0);
  } else {
    actionController(SLEEPING, val, true, false);
  }
}
#endif

void blynkSave(char* setting) {
  #if (BLYNK_ENABLE==1)
  if (!strcmp(setting, "Input")) { Blynk.virtualWrite(V2, String(Input, 2)); }
  else if (!strcmp(setting, "profile")) { Blynk.virtualWrite(V3, profile); }
  else if (!strcmp(setting, "aggKp")) { Blynk.virtualWrite(V4, String(aggKp, 1)); }
  else if (!strcmp(setting, "aggTn")) { Blynk.virtualWrite(V5, String(aggTn, 1)); }
  else if (!strcmp(setting, "aggTv")) { Blynk.virtualWrite(V6, String(aggTv, 1)); }
  else if (!strcmp(setting, "activeSetPoint")) { Blynk.virtualWrite(V7, String(*activeSetPoint, 1)); }
  else if (!strcmp(setting, "activeBrewTime")) { Blynk.virtualWrite(V8, String(*activeBrewTime, 1)); }
  else if (!strcmp(setting, "activePreinfusion")) { Blynk.virtualWrite(V9, String(*activePreinfusion, 1)); }
  else if (!strcmp(setting, "activePreinfusionPause")) { Blynk.virtualWrite(V10, String(*activePreinfusionPause, 1)); }
  else if (!strcmp(setting, "error")) { Blynk.virtualWrite(V11, String(Input - *activeSetPoint, 2)); }
  else if (!strcmp(setting, "activeStartTemp")) { Blynk.virtualWrite(V12, String(*activeStartTemp, 1)); }
  else if (!strcmp(setting, "pidON")) { Blynk.virtualWrite(V13, String(pidON)); }
  else if (!strcmp(setting, "output")) { Blynk.virtualWrite(V23, String(convertOutputToUtilisation(Output), 2)); }
  else if (!strcmp(setting, "aggoKp")) { Blynk.virtualWrite(V30, String(aggoKp, 1)); }
  else if (!strcmp(setting, "aggoTn")) { Blynk.virtualWrite(V31, String(aggoTn, 1)); }
  else if (!strcmp(setting, "aggoTv")) { Blynk.virtualWrite(V32, String(aggoTv, 1)); }
  else if (!strcmp(setting, "brewDetectionSensitivity")) { Blynk.virtualWrite(V34, String(brewDetectionSensitivity, 1)); }
  else if (!strcmp(setting, "pastTemperatureChange")) { Blynk.virtualWrite(V35, String(pastTemperatureChange(10*10) / 2, 2)); }
  else if (!strcmp(setting, "brewDetectionPower")) { Blynk.virtualWrite(V36, String(brewDetectionPower, 1)); }
  else if (!strcmp(setting, "steadyPower")) { Blynk.virtualWrite(V41, String(steadyPower, 1)); }
  else if (!strcmp(setting, "steadyPowerOffset")) { Blynk.virtualWrite(V42, String(steadyPowerOffset, 1)); }
  else if (!strcmp(setting, "steadyPowerOffsetTime")) { Blynk.virtualWrite(V43, String(steadyPowerOffsetTime, 1)); }
  else if (!strcmp(setting, "power_off_timer_min")) { Blynk.virtualWrite(V45, String(powerOffTimer >= 0 ? ((powerOffTimer + 59) / 60) : 0)); }
  else if (!strcmp(setting, "setPointSteam")) { Blynk.virtualWrite(V50, String(setPointSteam, 1)); }
  else if (!strcmp(setting, "cleaningCycles")) { Blynk.virtualWrite(V61, cleaningCycles); }
  else if (!strcmp(setting, "cleaningInterval")) { Blynk.virtualWrite(V62, cleaningInterval); }
  else if (!strcmp(setting, "cleaningPause")) { Blynk.virtualWrite(V63, cleaningPause); }
  else {
    ERROR_print("blynkSave(%s) not supported.\n", setting);
  }
  #endif
}

/******************************************************
 * Type Definition of "sending" BLYNK PIN values from
 * hardware to app/server (only defined if required)
 ******************************************************/
#if (BLYNK_ENABLE==1)
WidgetLED brewReadyLed(V14);
#endif

/******************************************************
 * HELPER
 ******************************************************/
bool isWifiWorking() {
#ifdef ESP32
  // DEBUG_print("status=%d ip=%s\n", WiFi.status() == WL_CONNECTED,
  // WiFi.localIP().toString());
  return ((!forceOffline) && (WiFi.status() == WL_CONNECTED) && (WiFi.localIP() != IPAddress(0U))); // TODO 2.7.x correct to remove IPAddress(0) check?
#else
  return ((!forceOffline) && (WiFi.status() == WL_CONNECTED) && (WiFi.localIP() != IPAddress(0U)));
#endif
}

bool isBlynkWorking() {
  #if (BLYNK_ENABLE==0)
    return false;
  #else
    return isWifiWorking() && Blynk.connected(); 
  #endif
}

bool inSensitivePhase() { return (brewing || activeState == 4 || isrCounter > 1000); }

int signnum(float x) {
  if (x >= 0.0)
    return 1;
  else
    return -1;
}

/********************************************************
 * Emergency Stop when temp too high
 *****************************************************/
void testEmergencyStop() {
  if (getCurrentTemperature() >= emergencyTemperature) {
    if (emergencyStop != true) {
      snprintf(debugLine, sizeof(debugLine), "EmergencyStop because temperature>%u (temperature=%0.2f)", emergencyTemperature, getCurrentTemperature());
      ERROR_println(debugLine);
      mqttPublish((char*)"events", debugLine);
      emergencyStop = true;
    }
  } else if (emergencyStop == true && getCurrentTemperature() < emergencyTemperature) {
    snprintf(debugLine, sizeof(debugLine), "EmergencyStop ended because temperature<%u (temperature=%0.2f)", emergencyTemperature, getCurrentTemperature());
    ERROR_println(debugLine);
    mqttPublish((char*)"events", debugLine);
    emergencyStop = false;
  }
}

/********************************************************
 * history temperature data
 *****************************************************/
void updateTemperatureHistory(float myInput) {
  readIndex++;
  if (readIndex >= numReadings) {
    readIndex = 0;
  }
  readingsTime[readIndex] = millis();
  readingsTemp[readIndex] = myInput;
}

// calculate the average temperature over the last (lookback) temperatures
// samples
float getAverageTemperature(int lookback, int offsetReading = 0) {
  float averageInput = 0;
  int count = 0;
  if (lookback >= numReadings) lookback = numReadings - 1;
  for (int offset = 0; offset < lookback; offset++) {
    int thisReading = (readIndex - offset - offsetReading) % numReadings;
    if (thisReading < 0) thisReading += numReadings;
    //DEBUG_print("getAverageTemperature(%d, %d): %d/%d = %0.2f\n", lookback, offsetReading, thisReading, readIndex, readingsTemp[thisReading]);
    if (readingsTime[thisReading] == 0) break;
    averageInput += readingsTemp[thisReading];
    count += 1;
  }
  if (count > 0) {
    return averageInput / count;
  } else {
    if (millis() > 60000) ERROR_print("getAverageTemperature(): no samples found\n");
    return 0;
  }
}

// calculate the temperature difference between NOW and a datapoint in history
float pastTemperatureChange(int lookback) {
   return pastTemperatureChange(lookback, true);
}
float pastTemperatureChange(int lookback, bool enable_avg) {
  // take 10samples (10*100ms = 1sec) for average calculations
  // thus lookback must be > avg_timeframe
  const int avg_timeframe = 10;  
  if (lookback >= numReadings) lookback = numReadings - 1;
  if (enable_avg) {
    int historicOffset = lookback - avg_timeframe;
    if (historicOffset < 0) return 0; //pastTemperatureChange will be 0 nevertheless
    float cur = getAverageTemperature(avg_timeframe);
    float past = getAverageTemperature(avg_timeframe, historicOffset);
    // ignore not yet initialized values
    if (cur == 0 || past == 0) return 0;
    return cur - past;
  } else {
    int historicIndex = (readIndex - lookback) % numReadings;
    if (historicIndex < 0) { historicIndex += numReadings; }
    // ignore not yet initialized values
    if (readingsTime[readIndex] == 0 || readingsTime[historicIndex] == 0) return 0;
    return readingsTemp[readIndex] - readingsTemp[historicIndex];
  }
}

float getCurrentTemperature() { return readingsTemp[readIndex]; }

float getTemperature(int lookback) {
  if (lookback >= numReadings) lookback = numReadings - 1;
  int offset = lookback % numReadings;
  int historicIndex = (readIndex - offset);
  if (historicIndex < 0) { historicIndex += numReadings; }
  // ignore not yet initialized values
  if (readingsTime[historicIndex] == 0) { return 0; }
  return readingsTemp[historicIndex];
}

// returns heater utilization in percent
float convertOutputToUtilisation(double Output) { return (100 * Output) / windowSize; }

// returns heater utilization in Output
double convertUtilisationToOutput(float utilization) { return (utilization / 100) * windowSize; }

bool checkBrewReady(float setPoint, float marginOfFluctuation, int lookback) {
  if (almostEqual(marginOfFluctuation, 0)) return false;
  if (lookback >= numReadings) lookback = numReadings - 1;
  for (int offset = 0; offset <= floor(lookback / 5); offset++) {
    int offsetReading = offset * 5;
    float temp_avg = getAverageTemperature(5, offsetReading);
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

float temperature_simulate_steam() {
  unsigned long now = millis();
  // if ( now <= 20000 ) return 102;
  // if ( now <= 26000 ) return 99;
  // if ( now <= 33000 ) return 96;
  // if (now <= 45000) return setPoint;  //TOBIAS remove
  if (now <= 20000) return 114;
  if (now <= 26000) return 117;
  if (now <= 29000) return 120;
  if (now <= 32000) return 116;
  if (now <= 35000) return 113;
  if (now <= 37000) return 109;
  if (now <= 39000) return 105;
  if (now <= 40000) return 101;
  if (now <= 43000) return 97;
  return *activeSetPoint;
}

float temperature_simulate_normal() {
  unsigned long now = millis();
  if (now <= 12000) return 82;
  if (now <= 15000) return 85;
  if (now <= 19000) return 88;
  if (now <= 25000) return 91;
  if (now <= 28000) return 92;
  return *activeSetPoint;
}

/********************************************************
 * check sensor value. If there is an issue, increase error value. 
 * If error is equal to maxErrorCounter, then set sensorMalfunction.
 * latestTemperature(=latest read sample) is read one sample (100ms) after secondlatestTemperature.
 * Returns: 0 := OK, 1 := Hardware issue, 2:= Software issue / outlier detected
 *****************************************************/
int checkSensor(float latestTemperature, float secondlatestTemperature) {
  int sensorStatus = 1;
  if (sensorMalfunction) {
    if (TempSensorRecovery == 1 && latestTemperature >= 0 && latestTemperature <= 150) {
      sensorMalfunction = false;
      error = 0;
      errorFarOff = 0;
      sensorStatus = 0;
      ERROR_print("temp sensor recovered\n");
    }
    return sensorStatus;
  }
  
  if (latestTemperature == 221) {
    error+=10;
    ERROR_print("temp sensor connection broken: consecErrors=%d, "
                "secondlatestTemperature=%0.2f, latestTemperature=%0.2f\n",
        error, secondlatestTemperature, latestTemperature);
  } else if (latestTemperature == 222) {
    error++;
    DEBUG_print("temp sensor read failed: consecErrors=%d, "
                "secondlatestTemperature=%0.2f, latestTemperature=%0.2f\n",
        error, secondlatestTemperature, latestTemperature);
  } else if (latestTemperature < 0 || latestTemperature > 150) {
    error++;
    DEBUG_print("temp sensor read unrealistic: consecErrors=%d, "
        "secondlatestTemperature=%0.2f, latestTemperature=%0.2f\n",
        error, secondlatestTemperature, latestTemperature); 
  } else if (fabs(latestTemperature - secondlatestTemperature) > 5) {
    error++;
    errorFarOff++;
    
    //support corner-case if due to some hangup the temperature jumps >5 degree.
    if (errorFarOff >= (maxErrorCounter/2)) {
      sensorStatus = 3;
      ERROR_print("temp sensor read far off fixed: consecErrors=%d, "
        "secondlatestTemperature=%0.2f, latestTemperature=%0.2f\n",
        error, secondlatestTemperature, latestTemperature);
    } else {
      DEBUG_print("temp sensor read far off: consecErrors=%d, "
        "secondlatestTemperature=%0.2f, latestTemperature=%0.2f\n",
        error, secondlatestTemperature, latestTemperature);
    }
#ifdef DEV_ESP
  } else if ((activeState==3 || activeState==1)  &&
     fabs(latestTemperature - secondlatestTemperature) >= 0.2 &&
     fabs(secondlatestTemperature - getTemperature(0)) >= 0.2 && 
     signnum(getTemperature(0) - secondlatestTemperature)*signnum(latestTemperature - secondlatestTemperature) > 0
     ) {
#else
  } else if (activeState==3 &&
     //fabs(secondlatestTemperature - setPoint) <= 5 &&
     fabs(latestTemperature - *activeSetPoint) <= 5 &&
     fabs(latestTemperature - secondlatestTemperature) >= 0.2 &&
     fabs(secondlatestTemperature - getTemperature(0)) >= 0.2 &&
     //fabs(latestTemperature - getTemperature(0)) <= 0.2 && //this check could be added also, but then return sensorStatus=1. 
     signnum(getTemperature(0) - secondlatestTemperature)*signnum(latestTemperature - secondlatestTemperature) > 0
     ) {
#endif
      error++;
      DEBUG_print("temp sensor inaccuracy: thirdlatestTemperature=%0.2f, secondlatestTemperature=%0.2f, latestTemperature=%0.2f\n",
        getTemperature(0), secondlatestTemperature, latestTemperature);
      sensorStatus = 2;
  } else {
    error = 0;
    errorFarOff = 0;
    sensorStatus = 0;
  }
  if (error >= maxErrorCounter) {
    sensorMalfunction = true;
    snprintf(debugLine, sizeof(debugLine), "temp sensor malfunction: latestTemperature=%0.2f, secondlatestTemperature=%0.2f", latestTemperature, secondlatestTemperature);
    ERROR_println(debugLine);
    mqttPublish((char*)"events", debugLine);
  }

  return sensorStatus;
}

/********************************************************
 * Refresh temperature.
 * Each time checkSensor() is called to verify the value.
 * If the value is not valid, new data is not stored.
 *****************************************************/
  void refreshTemp() {
    unsigned long refreshTimeDiff = millis() - previousTimerRefreshTemp;
    if (refreshTimeDiff >= refreshTempInterval) {
        if (refreshTimeDiff >= (refreshTempInterval *1.5)) {
          ERROR_print("refreshTemp(): Delay=%lu ms (loop() hang?)\n", refreshTimeDiff-refreshTempInterval);
        }
        //secondlatestTemperature = getCurrentTemperature();
        float latestTemperature = readTemperatureFromSensor();
        //DEBUG_print("latestTemperature: %0.2f\n", latestTemperature);
        // Temperatur_C = temperature_simulate_steam();
        // Temperatur_C = temperature_simulate_normal();
        int sensorStatus = checkSensor(latestTemperature, secondlatestTemperature);
        previousTimerRefreshTemp = millis();
        if (sensorStatus == 1) {  //hardware issue
          //DEBUG_print("(%d) latestTemperature=%0.2f (%0.2f) |UnchangedHist: -4=%0.2f, -3=%0.2f, -2=%0.2f, -1=%0.2f\n",
          //  sensorStatus, latestTemperature, secondlatestTemperature, getTemperature(3), getTemperature(2), getTemperature(1), getTemperature(0));
          //DEBUG_print("curTemp ERR: %0.2f\n", currentTemperature);
          return;
        } else if (sensorStatus == 2 || sensorStatus == 3) {  //software issue: outlier detected(==2) or temperature jump (==3)
          //DEBUG_print("latestTemperature saved (OUTLIER): %0.2f\n", latestTemperature);
          updateTemperatureHistory(latestTemperature);  //use currentTemp as replacement
        } else {
          updateTemperatureHistory(secondlatestTemperature);
        }
        //DEBUG_print("(%d) latestTemperature=%0.2f (%0.2f) |SavedHist: -4=%0.2f, -3=%0.2f, -2=%0.2f, -1=%0.2f\n",
        //  sensorStatus, latestTemperature, secondlatestTemperature, getTemperature(3), getTemperature(2), getTemperature(1), getTemperature(0));
        Input = getAverageTemperature(5*10);
        secondlatestTemperature = latestTemperature;
      }
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
          digitalWrite(pinRelayVentil, relayON);
          digitalWrite(pinRelayPumpe, relayON);
        } else {
          digitalWrite(pinRelayVentil, relayOFF);
          digitalWrite(pinRelayPumpe, relayOFF);
        }
      } else {
        if (cleaningEnableAutomatic && cycle < cleaningCycles) {
          brewStartTime = aktuelleZeit;
          cycle = cycle + 1;
        } else {
          DEBUG_print("End clean()\n");
          brewing = 0;
          cycle = 1;
          digitalWrite(pinRelayVentil, relayOFF);
          digitalWrite(pinRelayPumpe, relayOFF);
        }
      }
    } else if (simulatedBrewSwitch && !brewing) { // corner-case: switch=On but brewing==0
      waitingForBrewSwitchOff = true; // just to be sure
      brewTimer = 0;
    } else if (!simulatedBrewSwitch) {
      if (waitingForBrewSwitchOff) { DEBUG_print("simulatedBrewSwitch=off\n"); }
      waitingForBrewSwitchOff = false;
      if (brewing == 1) {
        digitalWrite(pinRelayVentil, relayOFF);
        digitalWrite(pinRelayPumpe, relayOFF);
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
    if (OnlyPID) { return; }
    unsigned long aktuelleZeit = millis();
    if (simulatedBrewSwitch && (brewing == 1 || waitingForBrewSwitchOff == false)) {

      if (brewing == 0) {
        brewing = 1; // Attention: For OnlyPID==0 brewing must only be changed in this function! Not externally.
        #if (SCALE_SENSOR_ENABLE)
        DEBUG_print("%lu HX711: Powerup\n", millis());
        scalePowerUp();
        DEBUG_print("%lu HX711: tareAsync\n", millis());
        tareAsync();
        #endif
        brewStartTime = aktuelleZeit;
        waitingForBrewSwitchOff = true; 
        
        totalBrewTime = ( BREWTIME_TIMER == 1 ? *activePreinfusion + *activePreinfusionPause + *activeBrewTime : *activeBrewTime ) * 1000;
        if (*activeBrewTimeEndDetection==1) totalBrewTime *= 2;  //never let it brew run longer than 2x totalBrewTime
      }

      brewTimer = aktuelleZeit - brewStartTime;

      if (aktuelleZeit >= lastBrewMessage + 1000) {
        lastBrewMessage = aktuelleZeit;
        //DEBUG_print("brew(%d): brewTimer=%02lu/%02lus (weight=%0.2fg/%0.2fg, tareCompleted=%d)\n", *activeBrewTimeEndDetection, brewTimer / 1000, totalBrewTime / 1000, currentWeight, *activeScaleSensorWeightSetPoint, LoadCell.getTareAsyncStatus());
        DEBUG_print("X brew(%u): brewTimer=%02lu/%02lus (weight=%0.2fg/%0.2fg) to=%d,%d,%d\n", *activeBrewTimeEndDetection, brewTimer / 1000, totalBrewTime / 1000, currentWeight, *activeScaleSensorWeightSetPoint, LoadCell.getTareTimeoutFlag(), LoadCell.getSignalTimeoutFlag(), scaleTareSuccess);
      }
      //DEBUG_print("X brew(%d): brewTimer=%02lu/%02lus (weight=%0.2fg/%0.2fg) to=%d,%d,%d\n", *activeBrewTimeEndDetection, brewTimer / 1000, totalBrewTime / 1000, currentWeight, *activeScaleSensorWeightSetPoint, LoadCell.getTareTimeoutFlag(), LoadCell.getSignalTimeoutFlag(), scaleTareSuccess;

      if (
        (brewTimer <= (*activeBrewTimeEndDetection == 0 || !getTareAsyncStatus() ) ? totalBrewTime : totalBrewTime *2) && 
        ( (*activeBrewTimeEndDetection == 1 && getTareAsyncStatus()) ? currentWeight <= *activeScaleSensorWeightSetPoint : true)
      ) {
        if (*activePreinfusion > 0 && brewTimer <= *activePreinfusion * 1000) {
          if (brewState != 1) {
            brewState = 1;
            DEBUG_println("preinfusion");
            digitalWrite(pinRelayVentil, relayON);
            digitalWrite(pinRelayPumpe, relayON);
          }
        } else if (*activePreinfusion > 0 && brewTimer > *activePreinfusion * 1000 && brewTimer <= (*activePreinfusion + *activePreinfusionPause) * 1000) {
          if (brewState != 2) {
            brewState = 2;
            DEBUG_println("preinfusion pause");
            digitalWrite(pinRelayVentil, relayON);
            digitalWrite(pinRelayPumpe, relayOFF);
          }
        } else if (*activePreinfusion == 0 || brewTimer > (*activePreinfusion + *activePreinfusionPause) * 1000) {
          if (brewState != 3) {
            brewState = 3;
            DEBUG_println("brew");
            digitalWrite(pinRelayVentil, relayON);
            digitalWrite(pinRelayPumpe, relayON);
          }
        }
      } else {
        brewState = 0;
        DEBUG_print("end brew()\n");
        brewing = 0;
        digitalWrite(pinRelayVentil, relayOFF);
        digitalWrite(pinRelayPumpe, relayOFF);
        #if (SCALE_SENSOR_ENABLE)
        scalePowerDown();
        currentWeight = 0;
        #endif
      }
    } else if (simulatedBrewSwitch && !brewing) { // corner-case: switch=On but brewing==0
      waitingForBrewSwitchOff = true; // just to be sure
      // digitalWrite(pinRelayVentil, relayOFF);  //already handled by brewing
      // var digitalWrite(pinRelayPumpe, relayOFF);
      brewTimer = 0;
      brewState = 0;
    } else if (!simulatedBrewSwitch) {
      if (waitingForBrewSwitchOff) { DEBUG_print("simulatedBrewSwitch=off\n"); }
      waitingForBrewSwitchOff = false;
      if (brewing == 1) {
        digitalWrite(pinRelayVentil, relayOFF);
        digitalWrite(pinRelayPumpe, relayOFF);
        brewing = 0;
        #if (SCALE_SENSOR_ENABLE)
        scalePowerDown();
        currentWeight = 0;
        #endif
      }
      brewTimer = 0;
      brewState = 0;
    }
  }


  /********************************************************
   * Check if Wifi is connected, if not reconnect
   *****************************************************/
  void checkWifi() { checkWifi(false, wifiConnectWaitTime); }
  void checkWifi(bool force_connect, unsigned long wifiConnectWaitTime_tmp) {
    if (forceOffline)
      return; // remove this to allow wifi reconnects even when
              // DISABLE_SERVICES_ON_STARTUP_ERRORS=1
    if ((!force_connect) && (isWifiWorking() || inSensitivePhase())) return;
    if (force_connect || (millis() > lastWifiConnectionAttempt + 5000 + (wifiReconnectInterval * wifiReconnects))) {
      lastWifiConnectionAttempt = millis();
      // noInterrupts();
      DEBUG_print("Connecting to WIFI with SID %s ...\n", ssid);
      WiFi.persistent(false); // Don't save WiFi configuration in flash
      WiFi.disconnect(true); // Delete SDK WiFi config
// displaymessage(0, "Connecting Wifi", "");
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
#ifdef ESP32
      //WiFi.setSleep(false);  //improve network performance. disabled because sometimes reboot happen?!
#else
      WiFi.setSleepMode(WIFI_NONE_SLEEP); // needed for some disconnection bugs?
#endif
      // WiFi.enableSTA(true);
      delay(100); // required for esp32?
      WiFi.setAutoConnect(false); // disable auto-connect
      WiFi.setAutoReconnect(false); // disable auto-reconnect
#ifdef ESP32
      WiFi.begin(ssid, pass);
      WiFi.setHostname(hostname); // TODO Reihenfolge wirklich richtig?
#else
    WiFi.hostname(hostname);
    WiFi.begin(ssid, pass);
#endif

      while (!isWifiWorking() && (millis() < lastWifiConnectionAttempt + wifiConnectWaitTime_tmp)) {
        yield(); // Prevent Watchdog trigger
      }
      if (isWifiWorking()) {
        DEBUG_print("Wifi connection attempt (#%u) successfull (%lu secs)\n", wifiReconnects, (millis() - lastWifiConnectionAttempt) / 1000);
        wifiReconnects = 0;
      } else {
        ERROR_print("Wifi connection attempt (#%u) not successfull (%lu secs)\n", wifiReconnects, (millis() - lastWifiConnectionAttempt) / 1000);
        wifiReconnects++;
      }
    }
  }

  /********************************************************
   * send data to Blynk server
   *****************************************************/
  void sendToBlynk() {
    #if (BLYNK_ENABLE==0)
      return;
    #else
      if (forceOffline || !isBlynkWorking() || blynkDisabledTemporary) return;
      unsigned long currentMillisBlynk = millis();
      if (currentMillisBlynk >= previousTimerBlynk + intervalBlynk) {
        previousTimerBlynk = currentMillisBlynk;
        if (brewReady) {
          if (blynkReadyLedColor != (char*)BLYNK_GREEN) {
            blynkReadyLedColor = (char*)BLYNK_GREEN;
            brewReadyLed.setColor(blynkReadyLedColor);
          }
        } else if (marginOfFluctuation != 0 && checkBrewReady(*activeSetPoint, marginOfFluctuation * 2, 40*10)) {
          if (blynkReadyLedColor != (char*)BLYNK_YELLOW) {
            blynkReadyLedColor = (char*)BLYNK_YELLOW;
            brewReadyLed.setColor(blynkReadyLedColor);
          }
        } else {
          if (blynkReadyLedColor != (char*)BLYNK_RED) {
            brewReadyLed.on();
            blynkReadyLedColor = (char*)BLYNK_RED;
            brewReadyLed.setColor(blynkReadyLedColor);
          }
        }
        if (grafana == 1 && blynkSendCounter == 1) { Blynk.virtualWrite(V60, Input, Output, bPID.GetKp(), bPID.GetKi(), bPID.GetKd(), *activeSetPoint); }
        // performance tests has shown to only send one api-call per sendToBlynk()
        if (blynkSendCounter == 1) {
          if (steadyPower != steadyPowerSavedInBlynk) {
            blynkSave((char*)"steadyPower");  // auto-tuning params should be saved by Blynk.virtualWrite()
            steadyPowerSavedInBlynk = steadyPower;
          } else {
            blynkSendCounter++;
          }
        }
        if (blynkSendCounter == 2) {
          if (String(pastTemperatureChange(10*10) / 2, 2) != PreviousPastTemperatureChange) {
            blynkSave((char*)"pastTemperatureChange");
            PreviousPastTemperatureChange = String(pastTemperatureChange(10*10) / 2, 2);
          } else {
            blynkSendCounter++;
          }
        }
        if (blynkSendCounter == 3) {
          if (String(Input - *activeSetPoint, 2) != PreviousError) {
            blynkSave((char*)"error");
            PreviousError = String(Input - *activeSetPoint, 2);
          } else {
            blynkSendCounter++;
          }
        }
        if (blynkSendCounter == 4) {
          if (String(convertOutputToUtilisation(Output), 2) != PreviousOutputString) {
            blynkSave((char*)"output");
            PreviousOutputString = String(convertOutputToUtilisation(Output), 2);
          } else {
            blynkSendCounter++;
          }
        }
        if (blynkSendCounter == 5) {
          powerOffTimer = ENABLE_POWER_OFF_COUNTDOWN - ((millis() - lastBrewEnd) / 1000);
          int power_off_timer_min = powerOffTimer >= 0 ? ((powerOffTimer + 59) / 60) : 0;
          if (power_off_timer_min != previousPowerOffTimer) {
            blynkSave((char*)"power_off_timer_min");
            previousPowerOffTimer = power_off_timer_min;
          } else {
            blynkSendCounter++;
          }
        }
        if (blynkSendCounter >= 6) {
          if (String(Input, 2) != PreviousInputString) {
            blynkSave((char*)"Input");
            PreviousInputString = String(Input, 2);
          }
          blynkSendCounter = 0;
        }
        blynkSendCounter++;
      }
    #endif
  }

  /********************************************************
   * state Detection
   ******************************************************/
  void updateState() {
    switch (activeState) {
      case 1: // state 1 running, that means full heater power. Check if target
              // temp is reached
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
          activeState = 2;
        }
        break;
      }
      case 2: // state 2 running, that means heater is on steadyState and we
              // are waiting to temperature to stabilize
      {
        bPID.SetFilterSumOutputI(30);

        if ((Input - *activeSetPoint >= 0) || (Input - *activeSetPoint <= -20) || (Input - *activeSetPoint <= 0 && pastTemperatureChange(20*10) <= 0.3)
            || (Input - *activeSetPoint >= -1.0 && pastTemperatureChange(10*10) > 0.2) || (Input - *activeSetPoint >= -1.5 && pastTemperatureChange(10*10) >= 0.45) || !pidMode || sleeping
            || cleaning) {
          // auto-tune starttemp
          if (millis() < 400000 && steadyPowerOffsetActivateTime > 0 && pidMode && MachineColdOnStart && !steaming && !sleeping
              && !cleaning) { // ugly hack to only adapt setPoint after power-on
            float tempChange = pastTemperatureChange(10*10);
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
          activeState = 3;
          bPID.SetAutoTune(true);
        }
        break;
      }
      case 4: // state 4 running = Brew running
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
          timerBrewDetection = 0;
          activeState = 2;
          lastBrewEnd = millis();
        }
        break;
      }
      case 5: // state 5 in outerZone
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
          activeState = 3;
        }
        break;
      }
      case 6: // state 6 heat up because we want to steam
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
          activeState = 3;
        }
        break;
      }
      case 7: // state 7 sleep modus activated (no heater,..)
      {
        if (!sleeping) {
          snprintf(debugLine, sizeof(debugLine), "** End of Sleeping phase. Transition to state 3 (normal mode)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetAutoTune(true);
          bPID.SetMode(AUTOMATIC);
          activeState = 3;
        }
        break;
      }
      case 8: // state 8 clean modus activated
      {
        if (!cleaning) {
          snprintf(debugLine, sizeof(debugLine), "** End of Cleaning phase. Transition to state 3 (normal mode)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetAutoTune(true);
          activeState = 3;
        }
        break;
      }

      case 3: // normal PID mode
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
          activeState = 7;
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
          activeState = 8;
          break;
        }

        /* STATE 6 (Steam) DETECTION */
        if (steaming) {
          snprintf(debugLine, sizeof(debugLine), "Steaming Detected. Transition to state 6 (Steam)");
          // digitalWrite(pinRelayVentil, relayOFF);
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          if (*activeSetPoint != setPointSteam) {
            activeSetPoint = &setPointSteam;
            DEBUG_print("set activeSetPoint: %0.2f\n", *activeSetPoint);
          }
          activeState = 6;
          break;
        }

        /* STATE 1 (COLDSTART) DETECTION */
        /*XXX1 READD
        if (Input <= *activeStartTemp - coldStartStep1ActivationOffset) {
          snprintf(debugLine, sizeof(debugLine), "** End of normal mode. Transition to state 1 (coldstart)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          steadyPowerOffsetActivateTime = millis();
          DEBUG_print("Enable steadyPowerOffset (%0.2f)\n", steadyPowerOffset);
          bPID.SetAutoTune(false); // do not tune during coldstart + phase2
          bPID.SetSumOutputI(0);
          activeState = 1;
          break;
        }
        */

        /* STATE 4 (BREW) DETECTION */
        if (brewDetection == 1 || (brewDetectionSensitivity != 0 && brewDetection == 2)) {
          // enable brew-detection if not already running and diff temp is >
          // brewDetectionSensitivity
          if (brewing
              || (OnlyPID && brewDetection == 2 && (pastTemperatureChange(3*10) <= -brewDetectionSensitivity) && fabs(getTemperature(5*10) - *activeSetPoint) <= outerZoneTemperatureDifference
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
            activeState = 4;
            break;
          }
        }

        /* STATE 5 (OUTER ZONE) DETECTION */
        if (Input > *activeStartTemp - coldStartStep1ActivationOffset && (fabs(Input - *activeSetPoint) > outerZoneTemperatureDifference) && !cleaning) {
          snprintf(debugLine, sizeof(debugLine), "** End of normal mode. Transition to state 5 (outerZone)");
          DEBUG_println(debugLine);
          mqttPublish((char*)"events", debugLine);
          bPID.SetSumOutputI(0);
          activeState = 5;
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
    if (activeState == 1 || activeState == 2 || activeState == 4) { Output_save = Output; }
    int ret = bPID.Compute();
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
      if (activeState == 1 || activeState == 2 || activeState == 4) {
#pragma GCC diagnostic error "-Wuninitialized"
        Output = Output_save;
      }
      DEBUG_print("Input=%6.2f | error=%5.2f delta=%5.2f | Output=%6.2f = b:%5.2f + "
                  "p:%5.2f + i:%5.2f(%5.2f) + d:%5.2f\n",
          Input, (*activeSetPoint - Input), pastTemperatureChange(10*10) / 2, convertOutputToUtilisation(Output), steadyPower + bPID.GetSteadyPowerOffsetCalculated(),
          convertOutputToUtilisation(bPID.GetOutputP()), convertOutputToUtilisation(bPID.GetSumOutputI()), convertOutputToUtilisation(bPID.GetOutputI()),
          convertOutputToUtilisation(bPID.GetOutputD()));
    } else if (ret == 2) { // PID is disabled but compute() should have run
      isrCounter = 0;
      pidComputeLastRunTime = millis();
      DEBUG_print("Input=%6.2f | error=%5.2f delta=%5.2f | Output=%6.2f (PID "
                  "disabled)\n",
          Input, (*activeSetPoint - Input), pastTemperatureChange(10*10) / 2, convertOutputToUtilisation(Output));
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
  void ICACHE_RAM_ATTR onTimer1ISR() {
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

  /***********************************
   * LOOP()
   ***********************************/
  void loop() {
    refreshTemp(); // save new temperature values
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
        checkWifi();
      } else {
        static bool runOnceOTASetup = true;
        if (runOnceOTASetup) {
          runOnceOTASetup = false;
          // Disable interrupt when OTA starts, otherwise it will not work
          ArduinoOTA.onStart([]() {
            DEBUG_print("OTA update initiated\n");
            Output = 0;
#ifdef ESP32
            timerAlarmDisable(timer);
#else
          timer1_disable();
#endif
            digitalWrite(pinRelayHeater, LOW); // Stop heating
          });
          ArduinoOTA.onError([](ota_error_t error) {
            ERROR_print("OTA update error\n");
#ifdef ESP32
            timerAlarmEnable(timer);
#else
          timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
#endif
          });
          // Enable interrupts if OTA is finished
          ArduinoOTA.onEnd([]() {
#ifdef ESP32
            timerAlarmEnable(timer);
#else
          timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
#endif
          });
        }
        if (millis() >= previousTimerOtaHandle + 500) {
          previousTimerOtaHandle = millis();
          ArduinoOTA.handle();
        }

        #if (BLYNK_ENABLE==1)
        if (!blynkDisabledTemporary) {
          if (isBlynkWorking()) {
            if (millis() >= previousTimerBlynkHandle + 500) {
              previousTimerBlynkHandle = millis();
              Blynk.run(); // Do Blynk household stuff. (On reconnect after
                           // disconnect, timeout seems to be 5 seconds)
            }
          } else {
            unsigned long now = millis();
            if ((now > blynkLastReconnectAttemptTime + (blynkReconnectIncrementalBackoff * (blynkReconnectAttempts)))
                && now > allServicesLastReconnectAttemptTime + allservicesMinReconnectInterval && !inSensitivePhase()) {
              blynkLastReconnectAttemptTime = now;
              allServicesLastReconnectAttemptTime = now;
              ERROR_print("Blynk disconnected. Reconnecting...\n");
              if (Blynk.connect(2000)) { // Attempt to reconnect
                blynkLastReconnectAttemptTime = 0;
                blynkReconnectAttempts = 0;
                DEBUG_print("Blynk reconnected in %lu seconds\n", (millis() - now) / 1000);
              } else if (blynkReconnectAttempts < blynkMaxIncrementalBackoff) {
                blynkReconnectAttempts++;
              }
            }
          }
        }
        #endif

        // Check mqtt connection
        if (MQTT_ENABLE && !mqttDisabledTemporary) {
          if (!isMqttWorking()) {
            mqttReconnect(false);
          } else {
#if (MQTT_ENABLE == 1)
            if (millis() >= previousTimerMqttHandle + 200) {
              previousTimerMqttHandle = millis();
              mqttClient.loop(); // mqtt client connected, do mqtt housekeeping
            }
#endif
            unsigned long now = millis();
            if (now >= lastMQTTStatusReportTime + lastMQTTStatusReportInterval) {
              lastMQTTStatusReportTime = now;
              mqttPublish((char*)"temperature", number2string(Input));
              mqttPublish((char*)"temperatureAboveTarget", number2string((Input - *activeSetPoint)));
              mqttPublish((char*)"heaterUtilization", number2string(convertOutputToUtilisation(Output)));
              mqttPublish((char*)"pastTemperatureChange", number2string(pastTemperatureChange(10*10)));
              mqttPublish((char*)"brewReady", bool2string(brewReady));
              if (ENABLE_POWER_OFF_COUNTDOWN != 0) {
                powerOffTimer = ENABLE_POWER_OFF_COUNTDOWN - ((millis() - lastBrewEnd) / 1000);
                mqttPublish((char*)"powerOffTimer", int2string(powerOffTimer >= 0 ? ((powerOffTimer + 59) / 60) : 0)); // in minutes always rounded up
              }
              // mqttPublishSettings();  //not needed because we update live on occurence
            }
          }
        }
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
      displaymessage(0, (char*)"Calibrating", (char*)"check logs");
    }
    return;
#endif

    pidCompute(); // call PID for Output calculation

    checkControls(controlsConfig); // transform controls to actions

    if (activeState == 8) {
      clean();
    } else {
      // handle brewing if button is pressed (ONLYPID=0 for now, because
      // ONLYPID=1_with_BREWDETECTION=1 is handled in actionControl).
      // ideally brew() should be controlled in our state-maschine (TODO)
      brew();
    }

    // check if PID should run or not. If not, set to manuel and force output to
    // zero
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

    // Sicherheitsabfrage
    if (!sensorMalfunction && !emergencyStop && Input > 0) {
      updateState();

      /* state 1: Water is very cold, set heater to full power */
      if (activeState == 1) {
        Output = windowSize;

        /* state 2: ColdstartTemp reached. Now stabilizing temperature after
         * coldstart */
      } else if (activeState == 2) {
        // Output = convertUtilisationToOutput(steadyPower +
        // bPID.GetSteadyPowerOffsetCalculated());
        Output = convertUtilisationToOutput(steadyPower);

        /* state 4: Brew detected. Increase heater power */
      } else if (activeState == 4) {
        if (Input > *activeSetPoint + outerZoneTemperatureDifference) {
          Output = convertUtilisationToOutput(steadyPower + bPID.GetSteadyPowerOffsetCalculated());
        } else {
          Output = convertUtilisationToOutput(brewDetectionPower);
        }
        if (OnlyPID == 1) {
          if (timerBrewDetection == 1) { brewTimer = millis() - lastBrewTime; }
        }

        /* state 5: Outer Zone reached. More power than in inner zone */
      } else if (activeState == 5) {
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
      } else if (activeState == 6) {
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
              //DEBUG_print("steam: input=%0.2f, past2s=%0.2f HEATING\n", Input,  pastTemperatureChange(2*10));
              Output = windowSize;
            } else if (Input > setPointSteam && (pastTemperatureChange(2*10) < -0.05)) {
              // full heat when >setPointSteam BUT temp goes down!
              //DEBUG_print("steam: input=%0.2f, past2s=%0.2f HEATING ABOVE\n", Input,  pastTemperatureChange(2*10));
              Output = windowSize;
            } else {
              //DEBUG_print("steam: input=%0.2f, past2s=%0.2f\n", Input,  pastTemperatureChange(2*10));
              Output = 0;
            }
          }
        }

        /* state 7: Sleeping state active*/
      } else if (activeState == 7) {
        if (millis() - recurringOutput > 60000) {
          recurringOutput = millis();
          snprintf(debugLine, sizeof(debugLine), "sleeping...");
          DEBUG_println(debugLine);
        }
        Output = 0;

        /* state 8: Cleaning state active*/
      } else if (activeState == 8) {
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

    } else if (sensorMalfunction) {
      // Deactivate PID
      if (pidMode == 1) {
        pidMode = 0;
        bPID.SetMode(pidMode);
        Output = 0;
        if (millis() - recurringOutput > 15000) {
          ERROR_print("sensorMalfunction detected. Shutdown PID and heater\n");
          recurringOutput = millis();
        }
      }
      digitalWrite(pinRelayHeater, LOW); // Stop heating
      char line2[17];
      snprintf(line2, sizeof(line2), "Temp. %0.2f", getCurrentTemperature());
      displaymessage(0, (char*)"Check Temp. Sensor!", (char*)line2);

    } else if (emergencyStop) {
      // Deactivate PID
      if (pidMode == 1) {
        pidMode = 0;
        bPID.SetMode(pidMode);
        Output = 0;
        if (millis() - recurringOutput > 10000) {
          ERROR_print("emergencyStop detected. Shutdown PID and heater (temp=%0.2f)\n", getCurrentTemperature());
          recurringOutput = millis();
        }
      }
      digitalWrite(pinRelayHeater, LOW); // Stop heating
      char line2[17];
      snprintf(line2, sizeof(line2),
          "%0.0f\xB0"
          "C",
          getCurrentTemperature());
      displaymessage(0, (char*)"Emergency Stop!", (char*)line2);

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
      noInterrupts();
      sync_eeprom();
      interrupts();
    }
  }

  /***********************************
   * WATER LEVEL SENSOR & MAINTENANCE
   ***********************************/
  void maintenance() {
#if (WATER_LEVEL_SENSOR_ENABLE)
    if (millis() >= previousTimerWaterLevelCheck + waterSensorCheckTimer) {
      previousTimerWaterLevelCheck = millis();
      unsigned int water_level_measured = waterSensor.readRangeContinuousMillimeters();
      if (waterSensor.timeoutOccurred()) {
        ERROR_println("Water level sensor: TIMEOUT");
        snprintf(displayMessageLine1, sizeof(displayMessageLine1), "Water sensor defect");
      } else if (water_level_measured >= WATER_LEVEL_SENSOR_LOW_THRESHOLD) {
        DEBUG_print("Water level is low: %u mm (low_threshold: %u)\n", water_level_measured, WATER_LEVEL_SENSOR_LOW_THRESHOLD);
        snprintf(displayMessageLine1, sizeof(displayMessageLine1), "Water is low!");
      } else {
        displayMessageLine1[0] = '\0';
      }
    }
#endif
  }

  void debugWaterLevelSensor() {
#if (WATER_LEVEL_SENSOR_ENABLE)
    unsigned long start = millis();
    unsigned int water_level_measured = waterSensor.readRangeContinuousMillimeters();
    if (waterSensor.timeoutOccurred()) {
      ERROR_println("WATER_LEVEL_SENSOR: TIMEOUT");
    } else
      DEBUG_print("WATER_LEVEL_SENSOR: %u mm (low_threshold: %u) (took: %lu ms)\n", water_level_measured, WATER_LEVEL_SENSOR_LOW_THRESHOLD, millis() - start);
#endif
  }

  void performance_check() {
    loops += 1;
    curMicros = micros();
    if (maxMicros < curMicros - curMicrosPreviousLoop) { maxMicros = curMicros - curMicrosPreviousLoop; }
    if (curMicros >= lastReportMicros + 100000) { // 100ms
      snprintf(debugLine, sizeof(debugLine),
          "%lu loop() | loops/ms=%lu | spend_micros_last_loop=%lu | "
          "max_micros_since_last_report=%lu | avg_micros/loop=%lu",
          curMicros / 1000, loops / 100, (curMicros - curMicrosPreviousLoop), maxMicros, (curMicros - lastReportMicros) / loops);
      DEBUG_println(debugLine);
      lastReportMicros = curMicros;
      maxMicros = 0;
      loops = 0;
    }
    curMicrosPreviousLoop = curMicros;
  }

  float readTemperatureFromSensor() {
#if (TEMPSENSOR == 3)
    //this sensor's reading flaps 0.5degrees on every read. Also calculate averages to mitigate PID issues.
    float past_average = getAverageTemperature(3,0);
    if (past_average == 0) {
      return thermocouple.readCelsius();
    } else {
      return (3*getAverageTemperature(3,0) + thermocouple.readCelsius()) / 4.0;
    }
#else
    return TSIC.getTemp();
#endif
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
    blynkSave((char*)"activeBrewTimeEndDetection");  //XXX1 blynk todo
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
    DEBUG_print("pidON: %d | tempSensor: %s\n", pidON, TEMPSENSOR_NAME);
    printControlsConfig(controlsConfig);
    printMultiToggleConfig();
    printMenuConfig(menuConfig);
    DEBUG_print("========================\n");
  }

  /***********************************
   * SETUP()
   ***********************************/
  void setup() {
    bool eeprom_force_read = true;
    DEBUGSTART(115200);

// required for remoteDebug to work
#ifdef ESP32
    WiFi.mode(WIFI_STA);
#endif
    Debug.begin(hostname, Debug.DEBUG);
    Debug.setResetCmdEnabled(true); // Enable the reset command
    Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
    Debug.showColors(true); // Colors
    Debug.setSerialEnabled(true); // log to Serial also
    // Serial.setDebugOutput(true); // enable diagnostic output of WiFi
    // libraries
    Debug.setCallBackNewClient(&print_settings);

    /********************************************************
     * Define trigger type
     ******************************************************/
    if (triggerType) {
      relayON = HIGH;
      relayOFF = LOW;
    } else {
      relayON = LOW;
      relayOFF = HIGH;
    }

    /********************************************************
     * Init Pins
     ******************************************************/
    pinMode(pinRelayVentil, OUTPUT);
    digitalWrite(pinRelayVentil, relayOFF);
    pinMode(pinRelayPumpe, OUTPUT);
    digitalWrite(pinRelayPumpe, relayOFF);
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

    DEBUG_print("\nMachine: %s\nVersion: %s\n", MACHINE_TYPE, sysVersion);

#if defined(OVERWRITE_VERSION_DISPLAY_TEXT)
    displaymessage(0, (char*)DISPLAY_TEXT, (char*)OVERWRITE_VERSION_DISPLAY_TEXT);
#else
  displaymessage(0, (char*)DISPLAY_TEXT, (char*)sysVersion);
#endif

    delay(1000);

    controlsConfig = parseControlsConfig();
    configureControlsHardware(controlsConfig);

    menuConfig = parseMenuConfig();

    // if simulatedBrewSwitch is already "on" on startup, then we brew should
    // not start automatically
    if (simulatedBrewSwitch) {
      ERROR_print("brewswitch is already on. Dont brew until it is turned off.\n");
      waitingForBrewSwitchOff = true;
    }

    /********************************************************
     * Init PID
     ******************************************************/
    bPID.SetSampleTime(windowSize);
    bPID.SetOutputLimits(0, windowSize);
    bPID.SetMode(AUTOMATIC);

    /********************************************************
     * INIT SCALE
     ******************************************************/
#if (SCALE_SENSOR_ENABLE)
    initScale();
#endif

    /********************************************************
     * BLYNK & Fallback offline
     ******************************************************/
    if (!forceOffline) {
      checkWifi(true, 12000); // wait up to 12 seconds for connection
      if (!isWifiWorking()) {
        ERROR_print("Cannot connect to WIFI %s. Disabling WIFI\n", ssid);
        if (DISABLE_SERVICES_ON_STARTUP_ERRORS) {
          forceOffline = true;
          mqttDisabledTemporary = true;
          blynkDisabledTemporary = true;
          lastWifiConnectionAttempt = millis();
        }
        displaymessage(0, (char*)"Cant connect to Wifi", (char*)"");
        delay(1000);
      } else {
        DEBUG_print("IP address: %s\n", WiFi.localIP().toString().c_str());

#if (defined(ESP32) and defined(DEBUGMODE))
        WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
        //WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
        WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
#endif

// MQTT
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
          // displaymessage(0, "Cannt connect to MQTT", "");
          // delay(1000);
        } else {
          const bool useRetainedSettingsFromMQTT = true;
          if (useRetainedSettingsFromMQTT) {
            // read and use settings retained in mqtt and therefore dont use eeprom values
            eeprom_force_read = false;
            unsigned long started = millis();
            while (isMqttWorking() && (millis() < started + 3000)) // attention: delay might not
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
      MQTT_server_onData(mqtt_callback_2);
      if (MQTT_server_start(mqtt_service_port, max_subscriptions, max_retained_topics)) {
        if (!MQTT_local_subscribe((unsigned char*)topicSet, 0) || !MQTT_local_subscribe((unsigned char*)topicActions, 0)) {
          ERROR_print("Cannot subscribe to local MQTT service\n");
        }
      } else {
        if (DISABLE_SERVICES_ON_STARTUP_ERRORS) mqttDisabledTemporary = true;
        ERROR_print("Cannot create MQTT service. Disabling...\n");
        // displaymessage(0, "Cannt create MQTT service", "");
        // delay(1000);
      }
#endif

        #if (BLYNK_ENABLE==1)
          DEBUG_print("Connecting to Blynk ...\n");
          Blynk.config(blynkAuth, blynkAddress, blynkPort);
          if (!Blynk.connect(5000)) {
            if (DISABLE_SERVICES_ON_STARTUP_ERRORS) blynkDisabledTemporary = true;
            ERROR_print("Cannot connect to Blynk. Disabling...\n");
            // delay(1000);
          } else {
            DEBUG_print("Blynk is online, get latest values\n");
            unsigned long started = millis();
            while (isBlynkWorking() && (millis() < started + 2000)) {
              Blynk.run();
            }
            eeprom_force_read = false;
          }
        #endif
      }

    } else {
      DEBUG_print("Staying offline due to forceOffline=1\n");
    }

    /********************************************************
     * READ/SAVE EEPROM
     * get latest values from EEPROM if not already fetched from blynk or
     * remote mqtt-server Additionally this function honors changed values in
     * userConfig.h (changed userConfig.h values have priority). Some special
     * variables like profile-dependent ones are always fetched from eeprom.
     ******************************************************/
    #ifndef ESP32
    EEPROM.begin(432);
    #endif
    sync_eeprom(true, eeprom_force_read);

    set_profile();

    print_settings();

    /********************************************************
     * PUBLISH settings on MQTT (and wait for them to be processed!)
     * + SAVE settings on MQTT-server if MQTT_ENABLE==1
     ******************************************************/
    steadyPowerSaved = steadyPower;
    if (isMqttWorking()) {
      steadyPowerMQTTDisableUpdateUntilProcessed = steadyPower;
      steadyPowerMQTTDisableUpdateUntilProcessedTime = millis();
      mqttPublishSettings();
#if (MQTT_ENABLE == 1)
      unsigned long started = millis();
      while ((millis() < started + 5000) && (steadyPowerMQTTDisableUpdateUntilProcessed != 0)) {
        mqttClient.loop();
      }
#endif
    }

    /********************************************************
     * OTA
     ******************************************************/
    if (ota && !forceOffline) {
      // wifi connection is done during blynk connection
      ArduinoOTA.setHostname(hostname); //  Device name for OTA
      ArduinoOTA.setPassword(OTApass); //  Password for OTA
      ArduinoOTA.begin();
    }

    /********************************************************
     * movingaverage ini array
     ******************************************************/
    for (int thisReading = 0; thisReading < numReadings; thisReading++) {
      readingsTemp[thisReading] = 0;
      readingsTime[thisReading] = 0;
    }

    /********************************************************
     * TEMP SENSOR
     ******************************************************/
    // displaymessage(0, "Init. vars", "");
    isrCounter = 950; // required
#if (TEMPSENSOR == 2)    
    if (TSIC.begin() != true) { ERROR_println("TSIC Tempsensor cannot be initialized"); }
    delay(120);
#endif    
    while (true) {
      secondlatestTemperature = readTemperatureFromSensor();
      delay(refreshTempInterval);
      // secondlatestTemperature = temperature_simulate_steam();
      // secondlatestTemperature = temperature_simulate_normal();
      Input = readTemperatureFromSensor();
      // Input = temperature_simulate_steam();
      // Input = temperature_simulate_normal();
      if (checkSensor(Input, secondlatestTemperature) == 0) {
        updateTemperatureHistory(secondlatestTemperature);
        secondlatestTemperature = Input;
        break;
      }
      displaymessage(0, (char*)"Temp sensor defect", (char*)"");
      ERROR_print("Temp sensor defect. Cannot read consistent values. Retrying\n");
      delay(1000);
    }

    /********************************************************
     * WATER LEVEL SENSOR
     ******************************************************/
#if (WATER_LEVEL_SENSOR_ENABLE)
#ifdef ESP32
    Wire1.begin(WATER_LEVEL_SENSOR_SDA, WATER_LEVEL_SENSOR_SCL,
        100000); // Wire0 cannot be re-used due to core0 stickyness
    waterSensor.setBus(&Wire1);
#else
    Wire.begin();
    waterSensor.setBus(&Wire);
#endif
    waterSensor.setTimeout(300);
    if (!waterSensor.init()) {
      ERROR_println("Water level sensor cannot be initialized");
      displaymessage(0, (char*)"Water sensor defect", (char*)"");
    }
    // increased accuracy by increase timing budget to 200 ms
    waterSensor.setMeasurementTimingBudget(200000);
    // continuous timed mode
    waterSensor.startContinuous(ENABLE_CALIBRATION_MODE == 1 ? 1000 : waterSensorCheckTimer / 2);
#endif

    /********************************************************
     * REST INIT()
     ******************************************************/
    #if (SCALE_SENSOR_ENABLE)
    scalePowerDown();
    #endif
    setHardwareLed(0);
    setGpioAction(BREWING, 0);
    setGpioAction(STEAMING, 0);
    setGpioAction(HOTWATER, 0);
    // Initialisation MUST be at the very end of the init(), otherwise the time
    // comparison in loop() will have a big offset
    unsigned long currentTime = millis();
    previousTimerRefreshTemp = currentTime;
    previousTimerBlynk = currentTime + 800;
    lastMQTTStatusReportTime = currentTime + 300;
    pidComputeLastRunTime = currentTime;

    /********************************************************
     * Timer1 ISR - Initialisierung
     * TIM_DIV1 = 0,   //80MHz (80 ticks/us - 104857.588 us max)
     * TIM_DIV16 = 1,  //5MHz (5 ticks/us - 1677721.4 us max)
     * TIM_DIV256 = 3  //312.5Khz (1 tick = 3.2us - 26843542.4 us max)
     ******************************************************/
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
    DEBUG_print("End of setup()\n");
  }
