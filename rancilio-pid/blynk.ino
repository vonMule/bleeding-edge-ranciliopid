/********************************************************
 * Perfect Coffee PID
 * https://github.com/medlor/bleeding-edge-ranciliopid
 *****************************************************/
#include "rancilio-pid.h"
#include "blynk.h"
#include "controls.h"

unsigned long previousTimerBlynk = 0;
unsigned long blynkConnectTime = 0;
const long intervalBlynk = 1000;    // Update intervall to send data to the app
int blynkSendCounter = 1;
bool blynkSyncRunOnce = false;
bool blynkDisabledTemporary = false;
float steadyPowerSavedInBlynk = 0;
unsigned long previousTimerBlynkHandle = 0;

String PreviousError = "";
String PreviousOutputString = "";
String PreviousPastTemperatureChange = "";
String PreviousInputString = "";

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


#if (BLYNK_ENABLE==0)
    void blynkSave(char* setting) {};
    void sendToBlynk() {};
    bool setupBlynk() { return true; };
    void runBlynk() {};
    void disableBlynkTemporary() {};
    void setPreviousTimerBlynk(unsigned long prevTimer) {};
    bool isBlynkWorking() { return false; } 
#else

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
BLYNK_WRITE(V64) { if ((millis() - blynkConnectTime > 10000 )) *activeBrewTimeEndDetection = (unsigned int) param.asInt(); }
BLYNK_WRITE(V65) { if ((millis() - blynkConnectTime > 10000 )) *activeScaleSensorWeightSetPoint = param.asFloat(); }
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

bool isBlynkWorking() { 
  static bool val_blynk = false;
  static unsigned long lastCheckBlynk = 0;
  if (millis() > lastCheckBlynk + 100UL) {
    lastCheckBlynk = millis();
    val_blynk = ((BLYNK_ENABLE > 0) && (isWifiWorking()) && (Blynk.connected()));
  }
  return val_blynk;
}

/******************************************************
 * Type Definition of "sending" BLYNK PIN values from
 * hardware to app/server (only defined if required)
 ******************************************************/
WidgetLED brewReadyLed(V14);

void blynkSave(char* setting) {
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
  else if (!strcmp(setting, "pastTemperatureChange")) { Blynk.virtualWrite(V35, String(tempSensor.pastTemperatureChange(10*10) / 2, 2)); }
  else if (!strcmp(setting, "brewDetectionPower")) { Blynk.virtualWrite(V36, String(brewDetectionPower, 1)); }
  else if (!strcmp(setting, "steadyPower")) { Blynk.virtualWrite(V41, String(steadyPower, 1)); }
  else if (!strcmp(setting, "steadyPowerOffset")) { Blynk.virtualWrite(V42, String(steadyPowerOffset, 1)); }
  else if (!strcmp(setting, "steadyPowerOffsetTime")) { Blynk.virtualWrite(V43, String(steadyPowerOffsetTime, 1)); }
  else if (!strcmp(setting, "power_off_timer_min")) { Blynk.virtualWrite(V45, String(powerOffTimer >= 0 ? ((powerOffTimer + 59) / 60) : 0)); }
  else if (!strcmp(setting, "setPointSteam")) { Blynk.virtualWrite(V50, String(setPointSteam, 1)); }
  else if (!strcmp(setting, "cleaningCycles")) { Blynk.virtualWrite(V61, cleaningCycles); }
  else if (!strcmp(setting, "cleaningInterval")) { Blynk.virtualWrite(V62, cleaningInterval); }
  else if (!strcmp(setting, "cleaningPause")) { Blynk.virtualWrite(V63, cleaningPause); }
  else if (!strcmp(setting, "activeBrewTimeEndDetection")) { Blynk.virtualWrite(V64, String(*activeBrewTimeEndDetection, 1)); }
  else if (!strcmp(setting, "activeScaleSensorWeightSetPoint")) { Blynk.virtualWrite(V65, String(*activeScaleSensorWeightSetPoint, 1)); }
  else {
    ERROR_print("blynkSave(%s) not supported.\n", setting);
  }
}

  /********************************************************
   * send data to Blynk server
   *****************************************************/
  void sendToBlynk() {
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
        // performance tests has shown to only send one api-call per sendToBlynk()
        if (blynkSendCounter == 1) {
          blynkSendCounter++;
          if (steadyPower != steadyPowerSavedInBlynk) {
            blynkSave((char*)"steadyPower");  // auto-tuning params should be saved by Blynk.virtualWrite()
            steadyPowerSavedInBlynk = steadyPower;
            return;
          }
        }
        if (blynkSendCounter == 2) {
          blynkSendCounter++;
          if (String(tempSensor.pastTemperatureChange(10*10) / 2, 2) != PreviousPastTemperatureChange) {
            blynkSave((char*)"pastTemperatureChange");
            PreviousPastTemperatureChange = String(tempSensor.pastTemperatureChange(10*10) / 2, 2);
            return;
          }
        }
        if (blynkSendCounter == 3) {
          blynkSendCounter++;
          if (String(Input - *activeSetPoint, 2) != PreviousError) {
            blynkSave((char*)"error");
            PreviousError = String(Input - *activeSetPoint, 2);
            return;
          }
        }
        if (blynkSendCounter == 4) {
          blynkSendCounter++;
          if (String(convertOutputToUtilisation(Output), 2) != PreviousOutputString) {
            blynkSave((char*)"output");
            PreviousOutputString = String(convertOutputToUtilisation(Output), 2);
            return;
          }
        }
        if (blynkSendCounter == 5) {
          blynkSendCounter++;
          powerOffTimer = ENABLE_POWER_OFF_COUNTDOWN - ((millis() - lastBrewEnd) / 1000);
          int power_off_timer_min = powerOffTimer >= 0 ? ((powerOffTimer + 59) / 60) : 0;
          if (power_off_timer_min != previousPowerOffTimer) {
            blynkSave((char*)"power_off_timer_min");
            previousPowerOffTimer = power_off_timer_min;
            return;
          }
        }
        if (blynkSendCounter >= 6) {
          blynkSendCounter = 1;
          String currentInput = String(Input, 2);
          if (currentInput != PreviousInputString) {
            blynkSave((char*)"Input");
            PreviousInputString = currentInput;
          }
        }
    }
}

void runBlynk() {
    if (!blynkDisabledTemporary) {
        if (isBlynkWorking()) {
            if (millis() >= previousTimerBlynkHandle + 500) {
              previousTimerBlynkHandle = millis();
              Blynk.run(); // Do Blynk household stuff. (On reconnect after
                           // disconnect, timeout seems to be 5 seconds)
            }
        } else {
            unsigned long now = millis();
            if ((now > blynkLastReconnectAttemptTime + (blynkReconnectIncrementalBackoff * (blynkReconnectAttempts<=blynkMaxIncrementalBackoff? blynkReconnectAttempts:blynkMaxIncrementalBackoff)))
                && now > allServicesLastReconnectAttemptTime + allservicesMinReconnectInterval && !inSensitivePhase()) {
              blynkLastReconnectAttemptTime = now;
              allServicesLastReconnectAttemptTime = now;
              ERROR_print("Blynk disconnected. Reconnecting...\n");
              if (Blynk.connect(2000)) { // Attempt to reconnect
                blynkLastReconnectAttemptTime = 0;
                blynkReconnectAttempts = 0;
                DEBUG_print("Blynk reconnected in %lu seconds\n", (millis() - now) / 1000);
              } else {
                blynkReconnectAttempts++;
              }
            }
        }
    }
 }

 bool setupBlynk() {
     DEBUG_print("Connecting to Blynk ...\n");
     Blynk.config(blynkAuth, blynkAddress, blynkPort);
     if (!Blynk.connect(5000)) {
         if (DISABLE_SERVICES_ON_STARTUP_ERRORS)
             blynkDisabledTemporary = true;
         ERROR_print("Cannot connect to Blynk. Disabling...\n");
         // delay(1000);
         return true;
     }
     else {
         DEBUG_print("Blynk is online, get latest values\n");
         unsigned long started = millis();
         while (isBlynkWorking() && (millis() < started + 2000)) {
             Blynk.run();
         }
         return false;
     }
 }

void disableBlynkTemporary() {
  blynkDisabledTemporary = true;
}

void setPreviousTimerBlynk(unsigned long prevTimer) {
  previousTimerBlynk = prevTimer;
}

#endif