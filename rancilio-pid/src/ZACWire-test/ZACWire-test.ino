#include <Arduino.h>
#include <Preferences.h>
Preferences preferences;

#define ESP32
#define pinTemperature    2    // read temperature (TSIC)
uint8_t pin = pinTemperature;

#include "src/ZACwire-Library/ZACwire.h"
ZACwire<pinTemperature> TSIC;


uint16_t temperature = 0;
float Temperatur_C = 0;
volatile uint16_t temp_value[2] = {0};
double previousInput = 0;
double Input = 0;
unsigned long previousMillistemp;
unsigned long previousEeprom;

// eeprom
const int expected_eeprom_version = 6;
int pidON = 1 ;
double setPointSteam = 1;
unsigned int estimated_cycle_refreshTemp = 25;  // for my TSIC the hardware refresh happens every 76ms
int burstShot      = 1;   // this is 1, when the user wants to immediatly set the heater power to the value specified in burstPower
double burstPower  = 20;  // in percent
double steadyPowerOffset     = 1;  // heater power (in percent) which should be added to steadyPower during steadyPowerOffsetTime
int steadyPowerOffsetTime   = 1;  // timeframe (in ms) for which steadyPowerOffset_Activated should be active
// State 3: Inner Zone PID values
double aggKp = 2;
double aggTn = 2;
double aggTv = 2;
#if (aggTn == 0)
double aggKi = 0;
#else
double aggKi = aggKp / aggTn;
#endif
double aggKd = aggTv * aggKp;

// State 4: Brew PID values
// ... none ...
double brewDetectionPower = 2;

// State 5: Outer Zone Pid values
double aggoKp = 2;
double aggoTn = 2;
double aggoTv = 2;
#if (aggoTn == 0)
double aggoKi = 0;
#else
double aggoKi = aggoKp / aggoTn;
#endif
double aggoKd = aggoTv * aggoKp ;
const double outerZoneTemperatureDifference = 1;
double setPoint = 2;
double starttemp = 2;
double steamReadyTemp = 2;
unsigned long  lastBrewTime = 0 ;
double brewDetectionSensitivity = 0;
double brewtime          = 2;
double preinfusion       = 2;
double preinfusionpause  = 2;
double steadyPower = 2;

//config
#define STEADYPOWER 4.6 // Constant power of heater to accomodate against loss of heat to environment (in %)
#define STEADYPOWER_OFFSET_TIME 850   // If brew group is very cold, we need to put additional power to the heater:
#define STEADYPOWER_OFFSET 3.0        // How much power should be added? (in %) (Rancilio Silvia 5E: 2.9, ...)
#define SETPOINT 93           // Temperature SetPoint
#define SETPOINT_STEAM 123    // Steam Temperature SetPoint which is used for controlAction STEAMING (Rancilio: 121, ECM Casa Prima: 145, ...)
#define STARTTEMP 80.6        // If temperature is below this value then Coldstart state should be triggered
#define AGGKP 80        // Kp (Rancilio Silvia 5E: 80, ...)
#define AGGTN 11        // Tn (Rancilio Silvia 5E: 11, ...)
#define AGGTV 32        // Tv (Rancilio Silvia 5E: 32, ...)
#define AGGOKP 170      // Kp (Rancilio Silvia 5E: 170, ...)
#define AGGOTN 40       // Tn (Rancilio Silvia 5E: 40, ...)
#define AGGOTV 41       // Tv (Rancilio Silvia 5E: 40, ...)
#define PREINFUSION       1.5   // length of time of the pre-infusion (in sec). Set to "0" to disable pre-infusion
#define PREINFUSION_PAUSE 2     // length of time of the pre-infusion pause (time in between the pre-infussion and brewing) (in sec). Set to "0" to disable pre-infusion pause.
#define BREWTIME        30.0  // length of time of the brewing (in sec). Used to limit brew time via hardware (ONLYPID=0) or visually (ONLYPID=1).
#define BREWDETECTION 2               // 0 = off | 1 = on using Hardware (ONLYPID=0 or ONLYPID=1_using_ControlAction_BREWING) | 2 = on using Software (ONLYPID=1 auto-detection)
#define BREWDETECTION_SENSITIVITY 0.6 // If temperature drops in the past 3 seconds by this number (Celcius), then a brew is detected (BREWDETECTION must be 1)
#define BREWDETECTION_WAIT 90         // (ONLYPID=1) 50=default: Software based BrewDetection is disabled after brewing for this number (seconds)
#define BREWDETECTION_POWER 75        // heater utiilization (in percent) during brew (BREWDETECTION must be 1)

int isrPin;
int _Sensortype;    //either 206, 306 or 506
static volatile byte BitCounter;
static volatile unsigned long ByteTime;
static volatile uint16_t rawTemp[2][2];
static volatile unsigned long deltaTime;  //TOBIAS remove volatile . also below!

static volatile  byte bitWindow; //TOBIAS remove volatile  . also below!
static volatile bool backUP;
    
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("start");
}

void loop() {
  unsigned long currentMillistemp = millis();
  if (currentMillistemp >= previousMillistemp + 500)
  {
    previousMillistemp = currentMillistemp;

    //ADRIAN CHANGE THIS
    //Input = getTemp(); 
    Input = TSIC.getTemp();

    Serial.print(millis()/1000);
    Serial.print(" temp=");
    Serial.println(Input);
  }
  if (currentMillistemp >= previousEeprom + 8000)
  {
    previousEeprom = currentMillistemp;
    sync_eeprom(false, true);
  }
}


static void IRAM_ATTR readTemp() {     //gets called with every rising edge
  static bool ByteNr;
  unsigned long microtime = micros();
    deltaTime = microtime - deltaTime;  //measure time to previous rising edge
    if (deltaTime > 1000) {       //true at start bit
      ByteTime = microtime;   //for measuring Tstrobe/bitWindow
    backUP = !backUP;
      BitCounter = ByteNr = rawTemp[0][backUP] = rawTemp[1][backUP] = 0;
    }
  if (++BitCounter == 11) {   //after stop bit
    ByteTime = microtime - ByteTime;
    ByteNr = 1;
  }
  rawTemp[ByteNr][backUP] <<= 1;
  if (deltaTime > bitWindow);   //Logic 0
  else if (deltaTime < bitWindow - 40 || rawTemp[ByteNr][backUP] & 2) rawTemp[ByteNr][backUP] |= 1; //Logic 1
    deltaTime = microtime;
}
  

bool begin() {      //start collecting data, needs to be called 100ms before the first getTemp()
    pinMode(pin, INPUT);
    bitWindow = 125;    //change from 0 to 125 to give the getTemp the info begin() was already executed
    deltaTime = micros();
    if (!pulseInLong(pin, LOW)) return false; //check if there is an incoming signal
    isrPin = digitalPinToInterrupt(pin);
    if (isrPin == -1) return false;
    attachInterrupt(isrPin, readTemp, RISING);
    return true;
}

float getTemp() {       //gives back temperature in °C
  static bool misreading = false;
  static byte newBitWindow;
  byte parity1 = 0, parity2 = 0;
  if (!bitWindow) { //check if begin() was already called
    begin();
    delay(110);
  }

  if (BitCounter != 20) misreading = true;  //use misreading-backup when newer reading is incomplete
  else newBitWindow = ((ByteTime << 5) + (ByteTime << 4) + ByteTime >> 9) + 20; //divide by 10.5 and add 20 (found out by trial and error)
  uint16_t tempHigh = rawTemp[0][backUP^misreading];    //get high significant bits from ISR
  uint16_t tempLow = rawTemp[1][backUP^misreading];   //get low   ''    ''
  if (abs(bitWindow-newBitWindow) < 20) bitWindow += (newBitWindow >> 3) - (bitWindow >> 3);  //adjust bitWindow time, which varies with rising temperature
  for (byte i = 0; i < 9; ++i) {
    if (tempHigh & 1 << i) ++parity1; //count "1" bits, which have to be even --> failure check
    if (tempLow & 1 << i) ++parity2;
  }
  if (tempHigh | tempLow && ~(parity1 | parity2) & 1) {       // check for failure
    tempHigh >>= 1;                   // delete parity bits
    tempLow >>= 1;
    tempLow |= tempHigh << 8;   //join high and low significant figures
    misreading = false;
    if (_Sensortype < 400) return (float(tempLow * 250L >> 8) - 499) / 10;  //calculates °C
    else return (float(tempLow * 175L >> 9) - 99) / 10;
  }
  else if (!misreading) {   //restart with backUP raw temperature
    misreading = true;
    getTemp();
  }
  else {
    misreading = false;
    return 222;   //set to 222 if reading failed
  }
}

void end() {
      detachInterrupt(isrPin);
}
    


    
void sync_eeprom(bool startup_read, bool force_read) {
  Serial.println("EEPROM: sync_eeprom");
  preferences.begin("config");
  int current_version = preferences.getInt("current_version", 0);
  if (current_version != expected_eeprom_version) {
    Serial.println("EEPROM: Version has changed or settings are corrupt or not previously set. Ignoring..");
    preferences.clear();
    preferences.putInt("current_version", expected_eeprom_version);
  }

  //if variables are not read from blynk previously, always get latest values from EEPROM
  if (force_read && (current_version == expected_eeprom_version)) {
    aggKp = preferences.getDouble("aggKp", 0.0);
    aggTn = preferences.getDouble("aggTn", 0.0);
    aggTv = preferences.getDouble("aggTv", 0.0);
    setPoint = preferences.getDouble("setPoint", 0.0);
    brewtime = preferences.getDouble("brewtime", 0.0);
    preinfusion = preferences.getDouble("preinfusion", 0.0f);
    preinfusionpause = preferences.getDouble("preinfusionpause", 0.0f);
    starttemp = preferences.getDouble("starttemp", 0.0f);
    aggoKp = preferences.getDouble("aggoKp", 0.0f);
    aggoTn = preferences.getDouble("aggoTn", 0.0f);
    aggoTv = preferences.getDouble("aggoTv", 0.0f);
    brewDetectionSensitivity = preferences.getDouble("brewDetectionSensitivity", 0.0f);
    steadyPower = preferences.getDouble("steadyPower", 0.0f);
    steadyPowerOffset = preferences.getDouble("steadyPowerOffset", 0.0f);
    steadyPowerOffsetTime = preferences.getInt("steadyPowerOffsetTime", 0);
    burstPower = preferences.getDouble("burstPower", 0.0f);
    brewDetectionPower = preferences.getDouble("brewDetectionPower", 0.0f);
    pidON = preferences.getInt("pidON");
    setPointSteam = preferences.getDouble("setPointSteam", 0.0f);
  }
  //always read the following values during setup() (which are not saved in blynk)
  if (startup_read && (current_version == expected_eeprom_version)) {
    estimated_cycle_refreshTemp = preferences.getUInt("estimated_cycle_refreshTemp");
  }

  //if blynk vars are not read previously, get latest values from EEPROM
  double aggKp_latest_saved = 0;
  double aggTn_latest_saved = 0;
  double aggTv_latest_saved = 0;
  double aggoKp_latest_saved = 0;
  double aggoTn_latest_saved = 0;
  double aggoTv_latest_saved = 0;
  double setPoint_latest_saved = 0;
  double brewtime_latest_saved = 0;
  double preinfusion_latest_saved = 0;
  double preinfusionpause_latest_saved = 0;
  double starttemp_latest_saved = 0;
  double brewDetectionSensitivity_latest_saved = 0;
  double steadyPower_latest_saved = 0;
  double steadyPowerOffset_latest_saved = 0;
  int steadyPowerOffsetTime_latest_saved = 0;
  double burstPower_latest_saved = 0;
  unsigned int estimated_cycle_refreshTemp_latest_saved = 0;
  double brewDetectionPower_latest_saved = 0;
  int pidON_latest_saved = 0;
  double setPointSteam_latest_saved = 0;

  if (current_version == expected_eeprom_version) {
    aggKp_latest_saved = preferences.getDouble("aggKp_latest_saved", 0.0f);
    aggTn_latest_saved = preferences.getDouble("aggTn_latest_saved", 0.0f);
    aggTv_latest_saved = preferences.getDouble("aggTv_latest_saved",0.0f);
    setPoint_latest_saved = preferences.getDouble("setPoint_latest_saved", 0.0f);
    brewtime_latest_saved = preferences.getDouble("brewtime_latest_saved", 0.0f);
    preinfusion_latest_saved = preferences.getDouble("preinfusion_latest_saved", 0.0f);
    preinfusionpause_latest_saved = preferences.getDouble("preinfusionpause_latest_saved", 0.0f);
    starttemp_latest_saved = preferences.getDouble("starttemp_latest_saved", 0.0f);
    aggoKp_latest_saved = preferences.getDouble("aggoKp_latest_saved", 0.0f);
    aggoTn_latest_saved = preferences.getDouble("aggoTn_latest_saved", 0.0f);
    aggoTv_latest_saved = preferences.getDouble("aggoTv_latest_saved", 0.0f);
    brewDetectionSensitivity_latest_saved = preferences.getDouble("brewDetectionSensitivity_latest_saved", 0.0f);
    steadyPower_latest_saved = preferences.getDouble("steadyPower_latest_saved", 0.0f);
    steadyPowerOffset_latest_saved = preferences.getDouble("steadyPowerOffset_latest_saved", 0.0f);
    steadyPowerOffsetTime_latest_saved = preferences.getInt("steadyPowerOffsetTime_latest_saved", 0);
    burstPower_latest_saved = preferences.getDouble("burstPower_latest_saved", 0.0f);
    estimated_cycle_refreshTemp_latest_saved = preferences.getUInt("estimated_cycle_refreshTemp_latest_saved");
    brewDetectionPower_latest_saved = preferences.getDouble("brewDetectionPower_latest_saved", 0.0f);
    pidON_latest_saved = preferences.getInt("pidON_latest_saved");
    setPointSteam_latest_saved = preferences.getDouble("setPointSteam_latest_saved", 0.0f);
  }

  //get saved userConfig.h values
  double aggKp_config_saved;
  double aggTn_config_saved;
  double aggTv_config_saved;
  double aggoKp_config_saved;
  double aggoTn_config_saved;
  double aggoTv_config_saved;
  double setPoint_config_saved;
  double brewtime_config_saved;
  double preinfusion_config_saved;
  double preinfusionpause_config_saved;
  double starttemp_config_saved;
  double brewDetectionSensitivity_config_saved;
  double steadyPower_config_saved;
  double steadyPowerOffset_config_saved;
  int steadyPowerOffsetTime_config_saved;
  double burstPower_config_saved;
  double brewDetectionPower_config_saved;
  double setPointSteam_config_saved;

  aggKp_config_saved = preferences.getDouble("aggKp_config_saved", 0.0f);
  aggTn_config_saved = preferences.getDouble("aggTn_config_saved", 0.0f);
  aggTv_config_saved = preferences.getDouble("aggTv_config_saved", 0.0f);
  setPoint_config_saved = preferences.getDouble("setPoint_config_saved", 0.0f);
  brewtime_config_saved = preferences.getDouble("brewtime_config_saved", 0.0f);
  preinfusion_config_saved = preferences.getDouble("preinfusion_config_saved", 0.0f);
  preinfusionpause_config_saved = preferences.getDouble("preinfusionpause_config_saved", 0.0f);
  starttemp_config_saved = preferences.getDouble("starttemp_config_saved", 0.0f);
  aggoKp_config_saved = preferences.getDouble("aggoKp_config_saved", 0.0f);
  aggoTn_config_saved = preferences.getDouble("aggoTn_config_saved", 0.0f);
  aggoTv_config_saved = preferences.getDouble("aggoTv_config_saved", 0.0f);
  brewDetectionSensitivity_config_saved = preferences.getDouble("brewDetectionSensitivity_config_saved", 0.0f);
  steadyPower_config_saved = preferences.getDouble("steadyPower_config_saved", 0.0f);
  steadyPowerOffset_config_saved = preferences.getDouble("steadyPowerOffset_config_saved", 0.0f);
  steadyPowerOffsetTime_config_saved = preferences.getInt("steadyPowerOffsetTime_config_saved");
  //burstPower_config_saved = preferences.getDouble("burstPower_config_saved", 0);
  brewDetectionPower_config_saved = preferences.getDouble("brewDetectionPower_config_saved", 0.0f);
  setPointSteam_config_saved = preferences.getDouble("setPointSteam_config_saved", 0.0f); 

  //use userConfig.h value if if differs from *_config_saved
  if (AGGKP != aggKp_config_saved) { aggKp = AGGKP; preferences.putDouble("aggKp", aggKp); }
  if (AGGTN != aggTn_config_saved) { aggTn = AGGTN; preferences.putDouble("aggTn", aggTn); }
  if (AGGTV != aggTv_config_saved) { aggTv = AGGTV; preferences.putDouble("aggTv", aggTv); }
  if (AGGOKP != aggoKp_config_saved) { aggoKp = AGGOKP; preferences.putDouble("aggoKp", aggoKp); }
  if (AGGOTN != aggoTn_config_saved) { aggoTn = AGGOTN; preferences.putDouble("aggoTn", aggoTn); }
  if (AGGOTV != aggoTv_config_saved) { aggoTv = AGGOTV; preferences.putDouble("aggoTv", aggoTv); }
  if (SETPOINT != setPoint_config_saved) { setPoint = SETPOINT; preferences.putDouble("setPoint", setPoint); Serial.println("EEPROM: setPoint is read from userConfig.h\n"); }
  if (BREWTIME != brewtime_config_saved) { brewtime = BREWTIME; preferences.putDouble("brewtime", brewtime); Serial.println("EEPROM: brewtime  is read from userConfig.h\n"); }
  if (PREINFUSION != preinfusion_config_saved) { preinfusion = PREINFUSION; preferences.putDouble("preinfusion", preinfusion); }
  if (PREINFUSION_PAUSE != preinfusionpause_config_saved) { preinfusionpause = PREINFUSION_PAUSE; preferences.putDouble("preinfusionpause", preinfusionpause); }
  if (STARTTEMP != starttemp_config_saved) { starttemp = STARTTEMP; preferences.putDouble("starttemp", starttemp); Serial.println("EEPROM: starttemp is read from userConfig.h\n"); }
  if (BREWDETECTION_SENSITIVITY != brewDetectionSensitivity_config_saved) { brewDetectionSensitivity = BREWDETECTION_SENSITIVITY; preferences.putDouble("brewDetectionSensitivity", brewDetectionSensitivity); }
  if (STEADYPOWER != steadyPower_config_saved) { steadyPower = STEADYPOWER; preferences.putDouble("steadyPower", steadyPower); }
  if (STEADYPOWER_OFFSET != steadyPowerOffset_config_saved) { steadyPowerOffset = STEADYPOWER_OFFSET; preferences.putDouble("steadyPowerOffset", steadyPowerOffset); }
  if (STEADYPOWER_OFFSET_TIME != steadyPowerOffsetTime_config_saved) { steadyPowerOffsetTime = STEADYPOWER_OFFSET_TIME; preferences.putInt("steadyPowerOffsetTime", steadyPowerOffsetTime); }
  //if (BURSTPOWER != burstPower_config_saved) { burstPower = BURSTPOWER; preferences.putDouble(470, burstPower); }
  if (BREWDETECTION_POWER != brewDetectionPower_config_saved) { brewDetectionPower = BREWDETECTION_POWER; preferences.putDouble("brewDetectionPower", brewDetectionPower); Serial.println("EEPROM: brewDetectionPower is read from userConfig.h\n"); }
  if (SETPOINT_STEAM != setPointSteam_config_saved) { setPointSteam = SETPOINT_STEAM; preferences.putDouble("setPointSteam", setPointSteam); Serial.println("EEPROM: setPointSteam  is read from userConfig.h\n"); }

  //save latest values to eeprom and sync back to blynk
  if ( aggKp != aggKp_latest_saved) { preferences.putDouble("aggKp", aggKp);  }
  if ( aggTn != aggTn_latest_saved) { preferences.putDouble("aggTn", aggTn); }
  if ( aggTv != aggTv_latest_saved) { preferences.putDouble("aggTv", aggTv); }
  if ( setPoint != setPoint_latest_saved) { preferences.putDouble("setPoint", setPoint);Serial.println("EEPROM: setPoint () is saved\n"); }
  if ( brewtime != brewtime_latest_saved) { preferences.putDouble("brewtime", brewtime);Serial.println("EEPROM: brewtime is saved (previous)\n"); }
  if ( preinfusion != preinfusion_latest_saved) { preferences.putDouble("preinfusion", preinfusion); }
  if ( preinfusionpause != preinfusionpause_latest_saved) { preferences.putDouble("preinfusionpause", preinfusionpause); }
  if ( starttemp != starttemp_latest_saved) { preferences.putDouble("starttemp", starttemp);Serial.println("EEPROM: starttemp  is saved\n"); }
  if ( aggoKp != aggoKp_latest_saved) { preferences.putDouble("aggoKp", aggoKp); ; }
  if ( aggoTn != aggoTn_latest_saved) { preferences.putDouble("aggoTn", aggoTn); }
  if ( aggoTv != aggoTv_latest_saved) { preferences.putDouble("aggoTv", aggoTv); }
  if ( brewDetectionSensitivity != brewDetectionSensitivity_latest_saved) { preferences.putDouble("brewDetectionSensitivity", brewDetectionSensitivity); }
  if ( steadyPower != steadyPower_latest_saved) { preferences.putDouble("steadyPower", steadyPower); Serial.println("EEPROM: steadyPower  is saved (previous:)\n"); }
  if ( steadyPowerOffset != steadyPowerOffset_latest_saved) { preferences.putDouble("steadyPowerOffset", steadyPowerOffset);  }
  if ( steadyPowerOffsetTime != steadyPowerOffsetTime_latest_saved) { preferences.putInt("steadyPowerOffsetTime", steadyPowerOffsetTime);}
  if ( burstPower != burstPower_latest_saved) { preferences.putDouble("burstPower", burstPower); }
  if ( estimated_cycle_refreshTemp != estimated_cycle_refreshTemp_latest_saved) { preferences.putUInt("estimated_cycle_refreshTemp", estimated_cycle_refreshTemp); }
  if ( brewDetectionPower != brewDetectionPower_latest_saved) { preferences.putDouble("brewDetectionPower", brewDetectionPower); }
  if ( pidON != pidON_latest_saved) { preferences.putInt("pidON", pidON);Serial.println("EEPROM: pidON  is saved \n"); }
  if ( setPointSteam != setPointSteam_latest_saved) { preferences.putDouble("setPointSteam", setPointSteam); Serial.println("EEPROM: setPointSteam () is saved\n"); }
  preferences.end();
  Serial.println("EEPROM: sync_eeprom() finished");
}
