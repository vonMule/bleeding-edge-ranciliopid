/********************************************************
 * Perfect Coffee PID
 * https://github.com/medlor/bleeding-edge-ranciliopid
 *****************************************************/

#include "eeprom-pcpid.h"
#include "rancilio-pid.h"

void sync_eeprom() { sync_eeprom(false, false); }

#ifdef ESP32
void sync_eeprom(bool startup_read, bool force_read) {
DEBUG_print("EEPROM: sync_eeprom(startup_read=%d, force_read=%d) called\n", startup_read, force_read);
preferences.begin("config");
int current_version = preferences.getInt("current_version", 0);
DEBUG_print("EEPROM: Detected Version=%d Expected Version=%d\n", current_version, expectedEepromVersion);
if (current_version != expectedEepromVersion) {
    ERROR_print("EEPROM: Version has changed or settings are corrupt or not previously set. Ignoring..\n");
    // preferences.clear();
    preferences.putInt("current_version", expectedEepromVersion);
}

// get latest version of profile dependent variables
if (startup_read && (current_version == expectedEepromVersion)) {
    setPoint1 = preferences.getDouble("setPoint1", 0.0);
    setPoint2 = preferences.getDouble("setPoint2", 0.0);
    setPoint3 = preferences.getDouble("setPoint3", 0.0);
    brewtime1 = preferences.getDouble("brewtime1", 0.0);
    brewtime2 = preferences.getDouble("brewtime2", 0.0);
    brewtime3 = preferences.getDouble("brewtime3", 0.0);
    preinfusion1 = preferences.getDouble("preinf1", 0.0);
    preinfusion2 = preferences.getDouble("preinf2", 0.0);
    preinfusion3 = preferences.getDouble("preinf3", 0.0);
    preinfusionpause1 = preferences.getDouble("preinfpau1", 0.0);
    preinfusionpause2 = preferences.getDouble("preinfpau2", 0.0);
    preinfusionpause3 = preferences.getDouble("preinfpau3", 0.0);
    starttemp1 = preferences.getDouble("starttemp1", 0.0);
    starttemp2 = preferences.getDouble("starttemp2", 0.0);
    starttemp3 = preferences.getDouble("starttemp3", 0.0);
}

// if variables are not read from blynk previously, always get latest values from EEPROM
if (force_read && (current_version == expectedEepromVersion)) {
    DEBUG_print("EEPROM: Blynk not active and not using external mqtt server. "
                "Reading settings from EEPROM\n");
    profile = preferences.getUInt("profile", 1);
    aggKp = preferences.getDouble("aggKp", 0.0);
    aggTn = preferences.getDouble("aggTn", 0.0);
    aggTv = preferences.getDouble("aggTv", 0.0);
    setPoint1 = preferences.getDouble("setPoint1", 0.0);
    setPoint2 = preferences.getDouble("setPoint2", 0.0);
    setPoint3 = preferences.getDouble("setPoint3", 0.0);
    brewtime1 = preferences.getDouble("brewtime1", 0.0);
    brewtime2 = preferences.getDouble("brewtime2", 0.0);
    brewtime3 = preferences.getDouble("brewtime3", 0.0);
    preinfusion1 = preferences.getDouble("preinf1", 0.0);
    preinfusion2 = preferences.getDouble("preinf2", 0.0);
    preinfusion3 = preferences.getDouble("preinf3", 0.0);
    preinfusionpause1 = preferences.getDouble("preinfpau1", 0.0);
    preinfusionpause2 = preferences.getDouble("preinfpau2", 0.0);
    preinfusionpause3 = preferences.getDouble("preinfpau3", 0.0);
    starttemp1 = preferences.getDouble("starttemp1", 0.0);
    starttemp2 = preferences.getDouble("starttemp2", 0.0);
    starttemp3 = preferences.getDouble("starttemp3", 0.0);
    aggoKp = preferences.getDouble("aggoKp", 0.0);
    aggoTn = preferences.getDouble("aggoTn", 0.0);
    aggoTv = preferences.getDouble("aggoTv", 0.0);
    brewDetectionSensitivity = preferences.getDouble("bDetSen", 0.0);
    steadyPower = preferences.getDouble("stePow", 0.0);
    steadyPowerOffset = preferences.getDouble("stePowOff", 0.0);
    steadyPowerOffsetTime = preferences.getUInt("stePowOT", 0);
    brewDetectionPower = preferences.getDouble("bDetPow", 0.0);
    pidON = preferences.getInt("pidON", 0) == 0 ? 0 : 1;
    setPointSteam = preferences.getDouble("sPointSte", 0.0);
    // cleaningCycles = preferences.getInt("clCycles", 0);
    // cleaningInterval = preferences.getInt("clInt", 0);
    // cleaningPause = preferences.getInt("clPause", 0);
}

// if blynk vars are not read previously, get latest values from EEPROM
unsigned int profile_sav = 0;
float aggKp_sav = 0;
float aggTn_sav = 0;
float aggTv_sav = 0;
float aggoKp_sav = 0;
float aggoTn_sav = 0;
float aggoTv_sav = 0;
float setPoint1_sav = 0;
float setPoint2_sav = 0;
float setPoint3_sav = 0;
float brewtime1_sav = 0;
float brewtime2_sav = 0;
float brewtime3_sav = 0;
float preinf1_sav = 0;
float preinf2_sav = 0;
float preinf3_sav = 0;
float preinfpau1_sav = 0;
float preinfpau2_sav = 0;
float preinfpau3_sav = 0;
float starttemp1_sav = 0;
float starttemp2_sav = 0;
float starttemp3_sav = 0;
float bDetSen_sav = 0;
float stePow_sav = 0;
float stePowOff_sav = 0;
unsigned int stePowOT_sav = 0;
float bDetPow_sav = 0;
int pidON_sav = 0;
float sPointSte_sav = 0;
// int clCycles_sav = 0;
// int clInt_sav = 0;
// int clPause_sav = 0;

if (current_version == expectedEepromVersion) {
    profile_sav = preferences.getUInt("profile", 1);
    aggKp_sav = preferences.getDouble("aggKp", 0.0);
    aggTn_sav = preferences.getDouble("aggTn", 0.0);
    aggTv_sav = preferences.getDouble("aggTv", 0.0);
    setPoint1_sav = preferences.getDouble("setPoint1", 0.0);
    setPoint2_sav = preferences.getDouble("setPoint2", 0.0);
    setPoint3_sav = preferences.getDouble("setPoint3", 0.0);
    brewtime1_sav = preferences.getDouble("brewtime1", 0);
    brewtime2_sav = preferences.getDouble("brewtime2", 0);
    brewtime3_sav = preferences.getDouble("brewtime3", 0);
    preinf1_sav = preferences.getDouble("preinf1", 0.0);
    preinf2_sav = preferences.getDouble("preinf2", 0.0);
    preinf3_sav = preferences.getDouble("preinf3", 0.0);
    preinfpau1_sav = preferences.getDouble("preinfpau1", 0.0);
    preinfpau2_sav = preferences.getDouble("preinfpau2", 0.0);
    preinfpau3_sav = preferences.getDouble("preinfpau3", 0.0);
    starttemp1_sav = preferences.getDouble("starttemp1", 0.0);
    starttemp2_sav = preferences.getDouble("starttemp2", 0.0);
    starttemp3_sav = preferences.getDouble("starttemp3", 0.0);
    aggoKp_sav = preferences.getDouble("aggoKp", 0.0);
    aggoTn_sav = preferences.getDouble("aggoTn", 0.0);
    aggoTv_sav = preferences.getDouble("aggoTv", 0.0);
    bDetSen_sav = preferences.getDouble("bDetSen", 0.0);
    stePow_sav = preferences.getDouble("stePow", 0.0);
    stePowOff_sav = preferences.getDouble("stePowOff", 0.0);
    stePowOT_sav = preferences.getUInt("stePowOT", 0);
    bDetPow_sav = preferences.getDouble("bDetPow", 0.0);
    pidON_sav = preferences.getInt("pidON", 0);
    sPointSte_sav = preferences.getDouble("sPointSte", 0.0);
    // clCycles_sav = preferences.getInt("clCycles", 0);
    // clInt_sav = preferences.getInt("clInt", 0);
    // clPause_sav = preferences.getInt("clPause", 0);
}

// get saved userConfig.h values
//unsigned int profile_cfg;
float aggKp_cfg;
float aggTn_cfg;
float aggTv_cfg;
float aggoKp_cfg;
float aggoTn_cfg;
float aggoTv_cfg;
float setPoint1_cfg;
float setPoint2_cfg;
float setPoint3_cfg;
float brewtime1_cfg;
float brewtime2_cfg;
float brewtime3_cfg;
float preinf1_cfg;
float preinf2_cfg;
float preinf3_cfg;
float preinfpau1_cfg;
float preinfpau2_cfg;
float preinfpau3_cfg;
float starttemp1_cfg;
float starttemp2_cfg;
float starttemp3_cfg;
float bDetSen_cfg;
float stePow_cfg;
float stePowOff_cfg;
unsigned int stePowOT_cfg;
float bDetPow_cfg;
float sPointSte_cfg;
// int clCycles_cfg;
// int clInt_cfg;
// int clPause_cfg;

//profile_cfg = preferences.getUInt("profile_cfg", 0.0);
aggKp_cfg = preferences.getDouble("aggKp_cfg", 0.0);
aggTn_cfg = preferences.getDouble("aggTn_cfg", 0.0);
aggTv_cfg = preferences.getDouble("aggTv_cfg", 0.0);
setPoint1_cfg = preferences.getDouble("setPoint1_cfg", 0.0);
setPoint2_cfg = preferences.getDouble("setPoint2_cfg", 0.0);
setPoint3_cfg = preferences.getDouble("setPoint3_cfg", 0.0);
brewtime1_cfg = preferences.getDouble("brewtime1_cfg", 0.0);
brewtime2_cfg = preferences.getDouble("brewtime2_cfg", 0.0);
brewtime3_cfg = preferences.getDouble("brewtime3_cfg", 0.0);
preinf1_cfg = preferences.getDouble("preinf1_cfg", 0.0);
preinf2_cfg = preferences.getDouble("preinf2_cfg", 0.0);
preinf3_cfg = preferences.getDouble("preinf3_cfg", 0.0);
preinfpau1_cfg = preferences.getDouble("preinfpau1_cfg", 0.0);
preinfpau2_cfg = preferences.getDouble("preinfpau2_cfg", 0.0);
preinfpau3_cfg = preferences.getDouble("preinfpau3_cfg", 0.0);
starttemp1_cfg = preferences.getDouble("starttemp1_cfg", 0.0);
starttemp2_cfg = preferences.getDouble("starttemp2_cfg", 0.0);
starttemp3_cfg = preferences.getDouble("starttemp3_cfg", 0.0);
aggoKp_cfg = preferences.getDouble("aggoKp_cfg", 0.0);
aggoTn_cfg = preferences.getDouble("aggoTn_cfg", 0.0);
aggoTv_cfg = preferences.getDouble("aggoTv_cfg", 0.0);
bDetSen_cfg = preferences.getDouble("bDetSen_cfg", 0.0);
stePow_cfg = preferences.getDouble("stePow_cfg", 0.0);
stePowOff_cfg = preferences.getDouble("stePowOff_cfg", 0.0);
stePowOT_cfg = preferences.getUInt("stePowOT_cfg");
bDetPow_cfg = preferences.getDouble("bDetPow_cfg", 0.0);
sPointSte_cfg = preferences.getDouble("sPointSte_cfg", 0.0);
// clCycles_cfg = preferences.getInt("clCycles_cfg");
// clInt_cfg = preferences.getInt("clInt_cfg");
// clPause_cfg = preferences.getInt("clPause_cfg");

// use userConfig.h value if if differs from *_cfg
if (!almostEqual(AGGKP, aggKp_cfg)) {
    aggKp = AGGKP;
    preferences.putDouble("aggKp_cfg", aggKp);
}
if (!almostEqual(AGGTN, aggTn_cfg)) {
    aggTn = AGGTN;
    preferences.putDouble("aggTn_cfg", aggTn);
}
if (!almostEqual(AGGTV, aggTv_cfg)) {
    aggTv = AGGTV;
    preferences.putDouble("aggTv_cfg", aggTv);
}
if (!almostEqual(AGGOKP, aggoKp_cfg)) {
    aggoKp = AGGOKP;
    preferences.putDouble("aggoKp_cfg", aggoKp);
}
if (!almostEqual(AGGOTN, aggoTn_cfg)) {
    aggoTn = AGGOTN;
    preferences.putDouble("aggoTn_cfg", aggoTn);
}
if (!almostEqual(AGGOTV, aggoTv_cfg)) {
    aggoTv = AGGOTV;
    preferences.putDouble("aggoTv_cfg", aggoTv);
}
if (!almostEqual(SETPOINT1, setPoint1_cfg)) {
    setPoint1 = SETPOINT1;
    preferences.putDouble("setPoint1_cfg", setPoint1);
    DEBUG_print("EEPROM: setPoint1 (%0.2f) is read from userConfig.h\n", setPoint1);
}
if (!almostEqual(SETPOINT2, setPoint2_cfg)) {
    setPoint2 = SETPOINT2;
    preferences.putDouble("setPoint2_cfg", setPoint2);
    DEBUG_print("EEPROM: setPoint2 (%0.2f) is read from userConfig.h\n", setPoint2);
}
if (!almostEqual(SETPOINT3, setPoint3_cfg)) {
    setPoint3 = SETPOINT3;
    preferences.putDouble("setPoint3_cfg", setPoint3);
    DEBUG_print("EEPROM: setPoint3 (%0.2f) is read from userConfig.h\n", setPoint3);
}
if (!almostEqual(BREWTIME1, brewtime1_cfg)) {
    brewtime1 = BREWTIME1;
    preferences.putDouble("brewtime1_cfg", brewtime1);
    DEBUG_print("EEPROM: brewtime1 (%0.2f) is read from userConfig.h (prev:%0.2f)\n", brewtime1, brewtime1_cfg);
}
if (!almostEqual(BREWTIME2, brewtime2_cfg)) {
    brewtime2 = BREWTIME2;
    preferences.putDouble("brewtime2_cfg", brewtime2);
    DEBUG_print("EEPROM: brewtime2 (%0.2f) is read from userConfig.h (prev:%0.2f)\n", brewtime2, brewtime2_cfg);
}
if (!almostEqual(BREWTIME3, brewtime3_cfg)) {
    brewtime3 = BREWTIME3;
    preferences.putDouble("brewtime3_cfg", brewtime3);
    DEBUG_print("EEPROM: brewtime3 (%0.2f) is read from userConfig.h (prev:%0.2f)\n", brewtime3, brewtime3_cfg);
}
if (!almostEqual(PREINFUSION1, preinf1_cfg)) {
    preinfusion1 = PREINFUSION1;
    preferences.putDouble("preinf1_cfg", preinfusion1);
}
if (!almostEqual(PREINFUSION2, preinf2_cfg)) {
    preinfusion2 = PREINFUSION2;
    preferences.putDouble("preinf2_cfg", preinfusion2);
}
if (!almostEqual(PREINFUSION3, preinf3_cfg)) {
    preinfusion3 = PREINFUSION3;
    preferences.putDouble("preinf3_cfg", preinfusion3);
}
if (!almostEqual(PREINFUSION_PAUSE1, preinfpau1_cfg)) {
    preinfusionpause1 = PREINFUSION_PAUSE1;
    preferences.putDouble("preinfpau1_cfg", preinfusionpause1);
}
if (!almostEqual(PREINFUSION_PAUSE2, preinfpau2_cfg)) {
    preinfusionpause2 = PREINFUSION_PAUSE2;
    preferences.putDouble("preinfpau2_cfg", preinfusionpause2);
}
if (!almostEqual(PREINFUSION_PAUSE3, preinfpau3_cfg)) {
    preinfusionpause3 = PREINFUSION_PAUSE3;
    preferences.putDouble("preinfpau3_cfg", preinfusionpause3);
}
if (!almostEqual(STARTTEMP1, starttemp1_cfg)) {
    starttemp1 = STARTTEMP1;
    preferences.putDouble("starttemp1_cfg", starttemp1);
    DEBUG_print("EEPROM: starttemp1 (%0.2f) is read from userConfig.h (prev:%0.2f)\n", starttemp1, starttemp1_cfg);
}
if (!almostEqual(STARTTEMP2, starttemp2_cfg)) {
    starttemp2 = STARTTEMP2;
    preferences.putDouble("starttemp2_cfg", starttemp2);
    DEBUG_print("EEPROM: starttemp2 (%0.2f) is read from userConfig.h (prev:%0.2f)\n", starttemp2, starttemp2_cfg);
}
if (!almostEqual(STARTTEMP3, starttemp3_cfg)) {
    starttemp3 = STARTTEMP3;
    preferences.putDouble("starttemp3_cfg", starttemp3);
    DEBUG_print("EEPROM: starttemp3 (%0.2f) is read from userConfig.h (prev:%0.2f)\n", starttemp3, starttemp3_cfg);
}
if (!almostEqual(BREWDETECTION_SENSITIVITY, bDetSen_cfg)) {
    brewDetectionSensitivity = BREWDETECTION_SENSITIVITY;
    preferences.putDouble("bDetSen_cfg", brewDetectionSensitivity);
}
if (!almostEqual(STEADYPOWER, stePow_cfg)) {
    steadyPower = STEADYPOWER;
    preferences.putDouble("stePow_cfg", steadyPower);
}
if (!almostEqual(STEADYPOWER_OFFSET, stePowOff_cfg)) {
    steadyPowerOffset = STEADYPOWER_OFFSET;
    preferences.putDouble("stePowOff_cfg", steadyPowerOffset);
}
if (!almostEqual(STEADYPOWER_OFFSET_TIME, stePowOT_cfg)) {
    steadyPowerOffsetTime = STEADYPOWER_OFFSET_TIME;
    preferences.putInt("stePowOT_cfg", steadyPowerOffsetTime);
}
if (!almostEqual(BREWDETECTION_POWER, bDetPow_cfg)) {
    brewDetectionPower = BREWDETECTION_POWER;
    preferences.putDouble("bDetPow_cfg", brewDetectionPower);
    DEBUG_print("EEPROM: brewDetectionPower (%0.2f) is read from userConfig.h\n", brewDetectionPower);
}
if (!almostEqual(SETPOINT_STEAM, sPointSte_cfg)) {
    setPointSteam = SETPOINT_STEAM;
    preferences.putDouble("sPointSte_cfg", setPointSteam);
    DEBUG_print("EEPROM: setPointSteam (%0.2f) is read from userConfig.h\n", setPointSteam);
}
// if (!almostEqual(CLEANING_CYCLES, clCycles_cfg)) { cleaningCycles = CLEANING_CYCLES;
// preferences.putInt("clCycles_cfg", cleaningCycles); } if
// (CLEANING_INTERVAL != clInt_cfg) { cleaningInterval = CLEANING_INTERVAL;
// preferences.putInt("clInt_cfg", cleaningInterval); } if (!almostEqual(CLEANING_PAUSE
// != clPause_cfg) { cleaningPause = CLEANING_PAUSE;
// preferences.putInt("clPause_cfg", cleaningPause); }

// save latest values to eeprom and sync back to blynk
if (profile != profile_sav) {
    preferences.putUInt("profile", profile);
    //blynkSave((char*)"profile");  //done in set_profile()
}
if (!almostEqual(aggKp, aggKp_sav)) {
    preferences.putDouble("aggKp", aggKp);
    blynkSave((char*)"aggKp");
}
if (!almostEqual(aggTn, aggTn_sav)) {
    preferences.putDouble("aggTn", aggTn);
    blynkSave((char*)"aggTn");
}
if (!almostEqual(aggTv, aggTv_sav)) {
    preferences.putDouble("aggTv", aggTv);
    blynkSave((char*)"aggTv");
}
if (!almostEqual(setPoint1, setPoint1_sav)) {
    preferences.putDouble("setPoint1", setPoint1);
    DEBUG_print("EEPROM: setPoint1 (%0.2f) is saved\n", setPoint1);
}
if (!almostEqual(setPoint2, setPoint2_sav)) {
    preferences.putDouble("setPoint2", setPoint2);
    DEBUG_print("EEPROM: setPoint2 (%0.2f) is saved (setPoint2_sav=%0.2f)\n", setPoint2, setPoint2_sav);
}
if (!almostEqual(setPoint3, setPoint3_sav)) {
    preferences.putDouble("setPoint3", setPoint3);
    DEBUG_print("EEPROM: setPoint3 (%0.2f) is saved\n", setPoint3);
}
if (!almostEqual(brewtime1, brewtime1_sav)) {
    preferences.putDouble("brewtime1", brewtime1);
    DEBUG_print("EEPROM: brewtime1 (%0.2f) is saved (previous:%0.2f)\n", brewtime1, brewtime1_sav);
}
if (!almostEqual(brewtime2, brewtime2_sav)) {
    preferences.putDouble("brewtime2", brewtime2);
    DEBUG_print("EEPROM: brewtime2 (%0.2f) is saved (previous:%0.2f)\n", brewtime2, brewtime2_sav);
}
if (!almostEqual(brewtime3, brewtime3_sav)) {
    preferences.putDouble("brewtime3", brewtime3);
    DEBUG_print("EEPROM: brewtime3 (%0.2f) is saved (previous:%0.2f)\n", brewtime3, brewtime3_sav);
}
if (!almostEqual(preinfusion1, preinf1_sav)) {
    preferences.putDouble("preinf1", preinfusion1);
}
if (!almostEqual(preinfusion2, preinf2_sav)) {
    preferences.putDouble("preinf2", preinfusion2);
}
if (!almostEqual(preinfusion3, preinf3_sav)) {
    preferences.putDouble("preinf3", preinfusion3);
}
if (!almostEqual(preinfusionpause1, preinfpau1_sav)) {
    preferences.putDouble("preinfpau1", preinfusionpause1);
}
if (!almostEqual(preinfusionpause2, preinfpau2_sav)) {
    preferences.putDouble("preinfpau2", preinfusionpause2);
}
if (!almostEqual(preinfusionpause3, preinfpau3_sav)) {
    preferences.putDouble("preinfpau3", preinfusionpause3);
}
if (!almostEqual(starttemp1, starttemp1_sav)) {
    preferences.putDouble("starttemp1", starttemp1);
    DEBUG_print("EEPROM: starttemp1 (%0.2f) is saved\n", starttemp1);
}
if (!almostEqual(starttemp2, starttemp2_sav)) {
    preferences.putDouble("starttemp2", starttemp2);
    DEBUG_print("EEPROM: starttemp2 (%0.2f) is saved\n", starttemp2);
}
if (!almostEqual(starttemp3, starttemp3_sav)) {
    preferences.putDouble("starttemp3", starttemp3);
    DEBUG_print("EEPROM: starttemp3 (%0.2f) is saved\n", starttemp3);
}
if (!almostEqual(aggoKp, aggoKp_sav)) {
    preferences.putDouble("aggoKp", aggoKp);
    blynkSave((char*)"aggoKp");
}
if (!almostEqual(aggoTn, aggoTn_sav)) {
    preferences.putDouble("aggoTn", aggoTn);
    blynkSave((char*)"aggoTn");
}
if (!almostEqual(aggoTv, aggoTv_sav)) {
    preferences.putDouble("aggoTv", aggoTv);
    blynkSave((char*)"aggoTv");
}
if (!almostEqual(brewDetectionSensitivity, bDetSen_sav)) {
    preferences.putDouble("bDetSen", brewDetectionSensitivity);
    blynkSave((char*)"brewDetectionSensitivity");
}
if (!almostEqual(steadyPower, stePow_sav)) {
    preferences.putDouble("stePow", steadyPower);
    blynkSave((char*)"steadyPower");
    DEBUG_print("EEPROM: steadyPower (%0.2f) is saved (previous:%0.2f)\n", steadyPower, stePow_sav);
}
if (!almostEqual(steadyPowerOffset, stePowOff_sav)) {
    preferences.putDouble("stePowOff", steadyPowerOffset);
    blynkSave((char*)"steadyPowerOffset");
}
if (steadyPowerOffsetTime != stePowOT_sav) {
    preferences.putInt("stePowOT", steadyPowerOffsetTime);
    blynkSave((char*)"steadyPowerOffsetTime");
}
if (!almostEqual(brewDetectionPower, bDetPow_sav)) {
    preferences.putDouble("bDetPow", brewDetectionPower);
    blynkSave((char*)"brewDetectionPower");
    DEBUG_print("EEPROM: brewDetectionPower (%0.2f) is saved (previous:%0.2f)\n", brewDetectionPower, bDetPow_sav);
}
if (pidON != pidON_sav) {
    preferences.putInt("pidON", pidON);
    blynkSave((char*)"pidON");
    DEBUG_print("EEPROM: pidON (%d) is saved (previous:%d)\n", pidON, pidON_sav);
}
if (!almostEqual(setPointSteam, sPointSte_sav)) {
    preferences.putDouble("sPointSte", setPointSteam);
    blynkSave((char*)"setPointSteam");
    DEBUG_print("EEPROM: setPointSteam (%0.2f) is saved\n", setPointSteam);
}
// if (!almostEqual( cleaningCycles, clCycles_sav)) { preferences.putInt("clCycles",
// cleaningCycles); Blynk.virtualWrite(V61, cleaningCycles); } if (
// cleaningInterval != clInt_sav) { preferences.putInt("clInt",
// cleaningInterval); Blynk.virtualWrite(V62, cleaningInterval); } if (
// cleaningPause != clPause_sav) { preferences.putInt("clPause",
// cleaningPause); Blynk.virtualWrite(V63, cleaningPause); }
preferences.end();
DEBUG_print("EEPROM: sync_eeprom() finished.\n");
}

#else
void sync_eeprom(bool startup_read, bool force_read) {
  int current_version;
  DEBUG_print("EEPROM: sync_eeprom(startup_read=%d, force_read=%d) called\n", startup_read, force_read);
  //EEPROM.begin(432);
  EEPROM.get(290, current_version);
  DEBUG_print("EEPROM: Detected Version=%d Expected Version=%d\n", current_version, expectedEepromVersion);
  if (current_version != expectedEepromVersion) {
    ERROR_print("EEPROM: Version has changed or settings are corrupt or not previously "
                "set. Ignoring..\n");
    EEPROM.put(290, expectedEepromVersion);
  }

  // get latest version of profile dependent variables
  if (startup_read && (current_version == expectedEepromVersion)) {
      EEPROM.get(76, setPoint1);
      EEPROM.get(80, setPoint2);
      EEPROM.get(84, setPoint3);
      //DEBUG_print("EEPROM: startup_read setPoint1=%0.2f, setPoint2=%0.2f, setPoint3=%0.2f\n", setPoint1, setPoint2, setPoint3);
      EEPROM.get(88, brewtime1);
      EEPROM.get(92, brewtime2);
      EEPROM.get(96, brewtime3);
      EEPROM.get(100, preinfusion1);
      EEPROM.get(104, preinfusion2);
      EEPROM.get(108, preinfusion3);
      EEPROM.get(112, preinfusionpause1);
      EEPROM.get(116, preinfusionpause2);
      EEPROM.get(120, preinfusionpause3);
      EEPROM.get(124, starttemp1);
      EEPROM.get(128, starttemp2);
      EEPROM.get(132, starttemp3);
  }

  // if variables are not read from blynk previously, always get latest values
  // from EEPROM
  if (force_read && (current_version == expectedEepromVersion)) {
    //DEBUG_print("EEPROM: Blynk not active and not using external mqtt server. Reading settings from EEPROM\n");
    EEPROM.get(0, aggKp);
    EEPROM.get(4, aggTn);
    EEPROM.get(8, aggTv);
    EEPROM.get(12, profile);
    //EEPROM.get(16, );
    //EEPROM.get(20, );
    //EEPROM.get(24, );
    //EEPROM.get(28, );
    EEPROM.get(32, aggoKp);
    EEPROM.get(36, aggoTn);
    EEPROM.get(40, aggoTv);
    EEPROM.get(44, brewDetectionSensitivity);
    EEPROM.get(48, steadyPower);
    EEPROM.get(52, steadyPowerOffset);
    EEPROM.get(56, steadyPowerOffsetTime);
    //EEPROM.get(60, burstPower);
    EEPROM.get(64, brewDetectionPower);
    EEPROM.get(68, pidON);
    EEPROM.get(72, setPointSteam);
    EEPROM.get(76, setPoint1);
    EEPROM.get(80, setPoint2);
    EEPROM.get(84, setPoint3);
    EEPROM.get(88, brewtime1);
    EEPROM.get(92, brewtime2);
    EEPROM.get(96, brewtime3);
    EEPROM.get(100, preinfusion1);
    EEPROM.get(104, preinfusion2);
    EEPROM.get(108, preinfusion3);
    EEPROM.get(112, preinfusionpause1);
    EEPROM.get(116, preinfusionpause2);
    EEPROM.get(120, preinfusionpause3);
    EEPROM.get(124, starttemp1);
    EEPROM.get(128, starttemp2);
    EEPROM.get(132, starttemp3);
    // Reminder: 290 is reserved for "version"
  }

  // if blynk vars are not read previously, get latest values from EEPROM
  float aggKp_sav = 0;
  float aggTn_sav = 0;
  float aggTv_sav = 0;
  float aggoKp_sav = 0;
  float aggoTn_sav = 0;
  float aggoTv_sav = 0;
  unsigned int profile_sav = 0;
  float setPoint1_sav = 0;
  float setPoint2_sav = 0;
  float setPoint3_sav = 0;
  float brewtime1_sav = 0;
  float brewtime2_sav = 0;
  float brewtime3_sav = 0;
  float preinf1_sav = 0;
  float preinf2_sav = 0;
  float preinf3_sav = 0;
  float preinfpau1_sav = 0;
  float preinfpau2_sav = 0;
  float preinfpau3_sav = 0;
  float starttemp1_sav = 0;
  float starttemp2_sav = 0;
  float starttemp3_sav = 0;
  float bDetSen_sav = 0;
  float stePow_sav = 0;
  float stePowOff_sav = 0;
  unsigned int stePowOT_sav = 0;
  float bDetPow_sav = 0;
  int pidON_sav = 0;
  float sPointSte_sav = 0;

  if (current_version == expectedEepromVersion) {
    EEPROM.get(0, aggKp_sav);
    EEPROM.get(4, aggTn_sav);
    EEPROM.get(8, aggTv_sav);
    EEPROM.get(12, profile_sav);
    //EEPROM.get(16, );
    //EEPROM.get(20, );
    //EEPROM.get(24, );
    //EEPROM.get(28, );
    EEPROM.get(32, aggoKp_sav);
    EEPROM.get(36, aggoTn_sav);
    EEPROM.get(40, aggoTv_sav);
    EEPROM.get(44, bDetSen_sav);
    EEPROM.get(48, stePow_sav);
    EEPROM.get(52, stePowOff_sav);
    EEPROM.get(56, stePowOT_sav);
    EEPROM.get(64, bDetPow_sav);
    EEPROM.get(68, pidON_sav);
    EEPROM.get(72, sPointSte_sav);
    EEPROM.get(76, setPoint1_sav);
    EEPROM.get(80, setPoint2_sav);
    EEPROM.get(84, setPoint3_sav);
    //DEBUG_print("EEPROM: saved setPoint1=%0.2f, setPoint2=%0.2f, setPoint3=%0.2f (%zu, %zu)\n", setPoint1_sav, setPoint2_sav, setPoint3_sav, sizeof(float), sizeof(float));
    EEPROM.get(88, brewtime1_sav);
    EEPROM.get(92, brewtime2_sav);
    EEPROM.get(96, brewtime3_sav);
    EEPROM.get(100, preinf1_sav);
    EEPROM.get(104, preinf2_sav);
    EEPROM.get(108, preinf3_sav);
    EEPROM.get(112, preinfpau1_sav);
    EEPROM.get(116, preinfpau2_sav);
    EEPROM.get(120, preinfpau3_sav);
    EEPROM.get(124, starttemp1_sav);
    EEPROM.get(128, starttemp2_sav);
    EEPROM.get(132, starttemp3_sav);
  }

  // get saved userConfig.h values
  float aggKp_cfg;
  float aggTn_cfg;
  float aggTv_cfg;
  float aggoKp_cfg;
  float aggoTn_cfg;
  float aggoTv_cfg;
  float setPoint1_cfg = 0;
  float setPoint2_cfg = 0;
  float setPoint3_cfg = 0;
  float brewtime1_cfg = 0;
  float brewtime2_cfg = 0;
  float brewtime3_cfg = 0;
  float preinf1_cfg = 0;
  float preinf2_cfg = 0;
  float preinf3_cfg = 0;
  float preinfpau1_cfg = 0;
  float preinfpau2_cfg = 0;
  float preinfpau3_cfg = 0;
  float starttemp1_cfg = 0;
  float starttemp2_cfg = 0;
  float starttemp3_cfg = 0;
  float bDetSen_cfg;
  float stePow_cfg;
  float stePowOff_cfg;
  unsigned int stePowOT_cfg;
  float bDetPow_cfg;
  float sPointSte_cfg;

  EEPROM.get(300, aggKp_cfg);
  EEPROM.get(304, aggTn_cfg);
  EEPROM.get(308, aggTv_cfg);
  /*
  EEPROM.get(312, profile_cfg);
  EEPROM.get(316, brewtime_cfg);
  EEPROM.get(320, preinf_cfg);
  EEPROM.get(324, preinfpau_cfg);
  EEPROM.get(328, starttemp_cfg);
  */
  EEPROM.get(332, aggoKp_cfg);
  EEPROM.get(336, aggoTn_cfg);
  EEPROM.get(340, aggoTv_cfg);
  EEPROM.get(344, bDetSen_cfg);
  EEPROM.get(348, stePow_cfg);
  EEPROM.get(352, stePowOff_cfg);
  EEPROM.get(356, stePowOT_cfg);
  //EEPROM.get(360, burstPower_cfg);
  EEPROM.get(364, bDetPow_cfg);
  EEPROM.get(368, sPointSte_cfg);
  EEPROM.get(372, setPoint1_cfg);
  EEPROM.get(376, setPoint2_cfg);
  EEPROM.get(380, setPoint3_cfg);
  //DEBUG_print("EEPROM: config setPoint1=%0.2f, setPoint2=%0.2f, setPoint3=%0.2f\n", setPoint1_cfg, setPoint2_cfg, setPoint3_cfg);
  EEPROM.get(384, brewtime1_cfg);
  EEPROM.get(388, brewtime2_cfg);
  EEPROM.get(392, brewtime3_cfg);
  EEPROM.get(396, preinf1_cfg);
  EEPROM.get(400, preinf2_cfg);
  EEPROM.get(404, preinf3_cfg);
  EEPROM.get(408, preinfpau1_cfg);
  EEPROM.get(412, preinfpau2_cfg);
  EEPROM.get(416, preinfpau3_cfg);
  EEPROM.get(420, starttemp1_cfg);
  EEPROM.get(424, starttemp2_cfg);
  EEPROM.get(428, starttemp3_cfg);

  // use userConfig.h value if if differs from *_cfg
  if (!almostEqual(AGGKP, aggKp_cfg)) {
    aggKp = AGGKP;
    EEPROM.put(300, aggKp);
  }
  if (!almostEqual(AGGTN, aggTn_cfg)) {
    aggTn = AGGTN;
    EEPROM.put(304, aggTn);
  }
  if (!almostEqual(AGGTV, aggTv_cfg)) {
    aggTv = AGGTV;
    EEPROM.put(308, aggTv);
  }
  if (!almostEqual(AGGOKP, aggoKp_cfg)) {
    aggoKp = AGGOKP;
    EEPROM.put(332, aggoKp);
  }
  if (!almostEqual(AGGOTN, aggoTn_cfg)) {
    aggoTn = AGGOTN;
    EEPROM.put(336, aggoTn);
  }
  if (!almostEqual(AGGOTV, aggoTv_cfg)) {
    aggoTv = AGGOTV;
    EEPROM.put(340, aggoTv);
  }
  if (!almostEqual(SETPOINT1, setPoint1_cfg)) {
    setPoint1 = SETPOINT1;
    EEPROM.put(372, setPoint1);
    DEBUG_print("EEPROM: setPoint1 (%0.2f) is read from userConfig.h\n", setPoint1);
  }
  if (!almostEqual(SETPOINT2, setPoint2_cfg)) {
    setPoint2 = SETPOINT2;
    EEPROM.put(376, setPoint2);
    DEBUG_print("EEPROM: setPoint2 (%0.2f) is read from userConfig.h\n", setPoint2);
  }
  if (!almostEqual(SETPOINT3, setPoint3_cfg)) {
    setPoint3 = SETPOINT3;
    EEPROM.put(380, setPoint3);
    DEBUG_print("EEPROM: setPoint3 (%0.2f) is read from userConfig.h\n", setPoint3);
  }
  if (!almostEqual(BREWTIME1, brewtime1_cfg)) {
    brewtime1 = BREWTIME1;
    EEPROM.put(384, brewtime1);
    DEBUG_print("EEPROM: brewtime1 (%0.2f) is read from userConfig.h\n", brewtime1);
  }
  if (!almostEqual(BREWTIME2, brewtime2_cfg)) {
    brewtime2 = BREWTIME2;
    EEPROM.put(388, brewtime2);
    DEBUG_print("EEPROM: brewtime2 (%0.2f) is read from userConfig.h\n", brewtime2);
  }
  if (!almostEqual(BREWTIME3, brewtime3_cfg)) {
    brewtime3 = BREWTIME3;
    EEPROM.put(392, brewtime3);
    DEBUG_print("EEPROM: brewtime3 (%0.2f) is read from userConfig.h\n", brewtime3);
  }
  if (!almostEqual(PREINFUSION1, preinf1_cfg)) {
    preinfusion1 = PREINFUSION1;
    EEPROM.put(396, preinfusion1);
  }
  if (!almostEqual(PREINFUSION2, preinf2_cfg)) {
    preinfusion2 = PREINFUSION2;
    EEPROM.put(400, preinfusion2);
  }
  if (!almostEqual(PREINFUSION3, preinf3_cfg)) {
    preinfusion3 = PREINFUSION3;
    EEPROM.put(404, preinfusion3);
  }
  if (!almostEqual(PREINFUSION_PAUSE1, preinfpau1_cfg)) {
    preinfusionpause1 = PREINFUSION_PAUSE1;
    EEPROM.put(408, preinfusionpause1);
  }
  if (!almostEqual(PREINFUSION_PAUSE2, preinfpau2_cfg)) {
    preinfusionpause2 = PREINFUSION_PAUSE2;
    EEPROM.put(412, preinfusionpause2);
  }
  if (!almostEqual(PREINFUSION_PAUSE3, preinfpau3_cfg)) {
    preinfusionpause3 = PREINFUSION_PAUSE3;
    EEPROM.put(416, preinfusionpause3);
  }
  if (!almostEqual(STARTTEMP1, starttemp1_cfg)) {
    starttemp1 = STARTTEMP1;
    EEPROM.put(420, starttemp1);
    DEBUG_print("EEPROM: starttemp1 (%0.2f) is read from userConfig.h\n", starttemp1);
  }
  if (!almostEqual(STARTTEMP2, starttemp2_cfg)) {
    starttemp2 = STARTTEMP2;
    EEPROM.put(424, starttemp2);
    DEBUG_print("EEPROM: starttemp2 (%0.2f) is read from userConfig.h\n", starttemp2);
  }
  if (!almostEqual(STARTTEMP3, starttemp3_cfg)) {
    starttemp3 = STARTTEMP3;
    EEPROM.put(428, starttemp3);
    DEBUG_print("EEPROM: starttemp3 (%0.2f) is read from userConfig.h\n", starttemp3);
  }
  if (!almostEqual(BREWDETECTION_SENSITIVITY, bDetSen_cfg)) {
    brewDetectionSensitivity = BREWDETECTION_SENSITIVITY;
    EEPROM.put(344, brewDetectionSensitivity);
  }
  if (!almostEqual(STEADYPOWER, stePow_cfg)) {
    steadyPower = STEADYPOWER;
    EEPROM.put(348, steadyPower);
  }
  if (!almostEqual(STEADYPOWER_OFFSET, stePowOff_cfg)) {
    steadyPowerOffset = STEADYPOWER_OFFSET;
    EEPROM.put(352, steadyPowerOffset);
  }
  if (!almostEqual(STEADYPOWER_OFFSET_TIME, stePowOT_cfg)) {
    steadyPowerOffsetTime = STEADYPOWER_OFFSET_TIME;
    EEPROM.put(356, steadyPowerOffsetTime);
  }
  if (!almostEqual(BREWDETECTION_POWER, bDetPow_cfg)) {
    brewDetectionPower = BREWDETECTION_POWER;
    EEPROM.put(364, brewDetectionPower);
    DEBUG_print("EEPROM: brewDetectionPower (%0.2f) is read from userConfig.h\n", brewDetectionPower);
  }
  if (!almostEqual(SETPOINT_STEAM, sPointSte_cfg)) {
    setPointSteam = SETPOINT_STEAM;
    EEPROM.put(368, setPointSteam);
    DEBUG_print("EEPROM: setPointSteam (%0.2f) is read from userConfig.h\n", setPointSteam);
  }

  // save latest values to eeprom and sync back to blynk
  if (!almostEqual(aggKp, aggKp_sav)) {
    EEPROM.put(0, aggKp);
    blynkSave((char*)"aggKp");
  }
  if (!almostEqual(aggTn, aggTn_sav)) {
    EEPROM.put(4, aggTn);
    blynkSave((char*)"aggTn");
  }
  if (!almostEqual(aggTv, aggTv_sav)) {
    EEPROM.put(8, aggTv);
    blynkSave((char*)"aggTv");
  }
  if (profile != profile_sav) {
    EEPROM.put(12, profile);
    blynkSave((char*)"profile");
    DEBUG_print("EEPROM: profile (%d) is saved (previous: %d)\n", profile, profile_sav);
  }
  if (!almostEqual(setPoint1, setPoint1_sav)) {
    EEPROM.put(76, setPoint1);
    DEBUG_print("EEPROM: setPoint1 (%0.2f) is saved (previous:%0.2f)\n", setPoint1, setPoint1_sav);
  }
  if (!almostEqual(setPoint2, setPoint2_sav)) {
    EEPROM.put(80, setPoint2);
    DEBUG_print("EEPROM: setPoint2 (%0.2f) is saved (previous:%0.2f)\n", setPoint2, setPoint2_sav);
  }
  if (!almostEqual(setPoint3, setPoint3_sav)) {
    EEPROM.put(84, setPoint3);
    DEBUG_print("EEPROM: setPoint3 (%0.2f) is saved (previous:%0.2f)\n", setPoint3, setPoint3_sav);
  }
  if (!almostEqual(brewtime1, brewtime1_sav)) {
    EEPROM.put(88, brewtime1);
    DEBUG_print("EEPROM: brewtime1 (%0.2f) is saved (previous:%0.2f)\n", brewtime1, brewtime1_sav);
  }
  if (!almostEqual(brewtime2, brewtime2_sav)) {
    EEPROM.put(92, brewtime2);
    DEBUG_print("EEPROM: brewtime2 (%0.2f) is saved (previous:%0.2f)\n", brewtime2, brewtime2_sav);
  }
  if (!almostEqual(brewtime3, brewtime3_sav)) {
    EEPROM.put(96, brewtime3);
    DEBUG_print("EEPROM: brewtime3 (%0.2f) is saved (previous:%0.2f)\n", brewtime3, brewtime3_sav);
  }
  if (!almostEqual(preinfusion1, preinf1_sav)) {
    EEPROM.put(100, preinfusion1);
  }
  if (!almostEqual(preinfusion2, preinf2_sav)) {
    EEPROM.put(104, preinfusion2);
  }
  if (!almostEqual(preinfusion3, preinf3_sav)) {
    EEPROM.put(108, preinfusion3);
  } 
  if (!almostEqual(preinfusionpause1, preinfpau1_sav)) {
    EEPROM.put(112, preinfusionpause1);
  }
  if (!almostEqual(preinfusionpause2, preinfpau2_sav)) {
    EEPROM.put(116, preinfusionpause2);
  }
  if (!almostEqual(preinfusionpause3, preinfpau3_sav)) {
    EEPROM.put(120, preinfusionpause3);
  }
  if (!almostEqual(starttemp1, starttemp1_sav)) {
    EEPROM.put(124, starttemp1);
    DEBUG_print("EEPROM: starttemp1 (%0.2f) is saved\n", starttemp1);
  }
  if (!almostEqual(starttemp2, starttemp2_sav)) {
    EEPROM.put(128, starttemp2);
    DEBUG_print("EEPROM: starttemp2 (%0.2f) is saved\n", starttemp2);
  }
  if (!almostEqual(starttemp3, starttemp3_sav)) {
    EEPROM.put(132, starttemp3);
    DEBUG_print("EEPROM: starttemp3 (%0.2f) is saved\n", starttemp3);
  }
  if (!almostEqual(aggoKp, aggoKp_sav)) {
    EEPROM.put(32, aggoKp);
    blynkSave((char*)"aggoKp");
  }
  if (!almostEqual(aggoTn, aggoTn_sav)) {
    EEPROM.put(36, aggoTn);
    blynkSave((char*)"aggoTn");
  }
  if (!almostEqual(aggoTv, aggoTv_sav)) {
    EEPROM.put(40, aggoTv);
    blynkSave((char*)"aggoTv");
  }
  if (!almostEqual(brewDetectionSensitivity, bDetSen_sav)) {
    EEPROM.put(44, brewDetectionSensitivity);
    blynkSave((char*)"brewDetectionSensitivity");
  }
  if (!almostEqual(steadyPower, stePow_sav)) {
    EEPROM.put(48, steadyPower);
    blynkSave((char*)"steadyPower");
    DEBUG_print("EEPROM: steadyPower (%0.2f) is saved (previous:%0.2f)\n", steadyPower, stePow_sav);
  }
  if (!almostEqual(steadyPowerOffset, stePowOff_sav)) {
    EEPROM.put(52, steadyPowerOffset);
    blynkSave((char*)"steadyPowerOffset");
  }
  if (steadyPowerOffsetTime != stePowOT_sav) {
    EEPROM.put(56, steadyPowerOffsetTime);
    blynkSave((char*)"steadyPowerOffsetTime");
  }
  if (!almostEqual(brewDetectionPower, bDetPow_sav)) {
    EEPROM.put(64, brewDetectionPower);
    blynkSave((char*)"brewDetectionPower");
    DEBUG_print("EEPROM: brewDetectionPower (%0.2f) is saved (previous:%0.2f)\n", brewDetectionPower, bDetPow_sav);
  }
  if (pidON != pidON_sav) {
    EEPROM.put(68, pidON);
    blynkSave((char*)"pidON");
    DEBUG_print("EEPROM: pidON (%d) is saved (previous:%d)\n", pidON, pidON_sav);
  }
  if (!almostEqual(setPointSteam, sPointSte_sav)) {
    EEPROM.put(72, setPointSteam);
    blynkSave((char*)"setPointSteam");
    DEBUG_print("EEPROM: setPointSteam (%0.2f) is saved\n", setPointSteam);
  }
  if (!EEPROM.commit()) ERROR_print("Cannot write to EEPROM.\n");
  DEBUG_print("EEPROM: sync_eeprom() finished.\n");
}
#endif
