# PERFECT COFFEE PID 
(former bleeding edge ranciliopid)

## Do it yourself, open-source PID for your espresso machine  

Version 3.2.1

## Support / Contact
You can chat with us directly using our [discord server](https://discord.gg/VA5ZeacFdw).

## Videos
[![Introduction](https://img.youtube.com/vi/dQWHeygS9ws/hqdefault.jpg)](https://www.youtube.com/watch?v=dQWHeygS9ws)
<details>
  <summary>Click for more videos!</summary>

| Description             |  Video |
:-----------------------------------------------:|:---------------------------------------------:
ESP32 Functions with full Softwarecontrol | [![ESP32 Functions with full Softwarecontrol](https://img.youtube.com/vi/rxVFKlREcDk/hqdefault.jpg)](https://www.youtube.com/watch?v=rxVFKlREcDk)
Cleaning Mode | [![Cleaning Mode](https://img.youtube.com/vi/qoNPq2WBcS0/hqdefault.jpg)](https://www.youtube.com/watch?v=qoNPq2WBcS0)
Waterlevel Sensor | [![Waterlevel Sensor](https://img.youtube.com/vi/JpUrzaUCCnQ/hqdefault.jpg)](https://www.youtube.com/watch?v=JpUrzaUCCnQ)
Two cups of a double cappuccino with the new Steamfunction | [![Two cups of a double cappuccino with the new Steamfunction](https://img.youtube.com/vi/dgLpD719_sw/hqdefault.jpg)](https://www.youtube.com/watch?v=dgLpD719_sw)
</details>

&nbsp;


## Most important features compared to the rancilio-pid software:
1. New PID Controller "Multi-state PID with steadyPower (Bias)"
   - Target-Temperature for brewing and steaming (!) is automatically controlled by PID.
   - Auto-Tuning of all PID settings. No knowledge or special tunings required.
   - Distinct PID settings dependend on the current "state" of the machine. 
   - Most of the settings are either static or semi-automatically tuned, which does not require an PHD (German: Diplom) to understand.
   - Currently 5 states are implemented:
     - Coldstart (machine is cold)
     - Coldstart stabilisation (directly after coldstart)
     - Inner Zone (temperature near setPoint)
     - Outer Zone (temperature outside of "inner zone")
     - Brewing
     - Steaming (!)
   - steadyPower is introduced which compensates the constant temperature loss due to environment
   - steadyPowerOffset is introduced which compensates the increased temperature loss when the machine (brew head etc.) is still very cold.
   - PidController offers feature like I-value filtering, special handling of setPoint crossings and more (hard-coded)
   - PID Controller is now integral part of the software and not an external library.
1. FULL ESP32 (and of cause esp8266) support in bleeding-edge main-branch.
1. Support of three hardware modifications which matches you requirements and offers the best flexibility.
   - PidOnly: This is the most basic modifcations in which only the heater is controlled by the esp.
   - Standard: In this modifcations in addition to the heater, "brewing" is also controlled by the esp which enables pre-infusion (by cutting the "hardware-button switched" power to the valve and the pump on demand).
   - FullControl: (ESP32 only) All functionality is directly controlled by the esp. This allows full customization and implementation of any feature. Also all functionality can therefore remotely be activated/deactivated. The ESP32 can run 24/7 if required.
1. Freely choose if you want the software to use WIFI, BLYNK and/or MQTT. Everythink can be enabled/disabled and still have a flawlessly working PID controller.
1. Additionally if you want to to depend on a remotely running service (eg. blynk server on raspi), you can activate a MQTT-Server on the arduino itself!
1. By using the actionController you can freely configure the system to trigger custom ACTIONS (=functions) when a GPIO port is triggered (eg by a switch) or when commands are received over network (mqtt/blynk).
   - fully dynamic mapping of any(!) available analog/digital gpio port to trigger following custom ACTIONS:
     - BREWING   := start brewing
     - STEAMING  := automatically manages heat up machine for steaming
     - HOTWATER  := start pouring hotwater
     - HEATER    := activate heater (not yet)
     - PUMP      := activate pump (not yet)
     - VALVE     := activate Valve  (not yet)
     - CLEANING  := activate cleaning mode
     - SLEEPING  := activate sleeping mode
     - MENU      := activate menu
     - MENU_INC  := increase value shown in menu
     - MENU_DEC  := decrease value shown in menu
   - while also supporting the switch types: toggles (eg switches) and triggers (eg push buttons)
   - added MQTT support to control ACTIONS using topics ../actions/<ACTION> with supported payloads of 0|1|-1 (off|on|switch)
     Example: "custom/Küche.Rancilio2/actions/STEAMING"
   - added Blynk support to control ACTIONS using virtual pins
1. MultiToggle triggers any ACTION based on multiple simultaneous hardware-button states (eg. if hotwater+steaming button is pressed then start the action CLEANING). MultiToggle can trigger up to six ACTIONS just by using the three rancilio's hardware-switches and therefore reduces the need for external control (mqtt, blynk, resistor-circuit, ..).
1. GpioAction: When actions are activated, specific GPIOs can be triggered also. This can be used eg. to light up LEDs in Rancilio's custom modified hardware-switches.
1. "Brew Profiles" can be used to easily set optimum brew settings for different beans.
1. Scale support to optionally brew by weight (in addition to by time).
1. Offline Modus is fixed and enhanced. If userConfig.h's FORCE_OFFLINE is enabled, then PID fully is working without networking. Ideal in situations when there is no connectivity or you dont want to rely on it.
1. Huge performance tunings and improvements under the hood which stabilizes the system (eg in situations of bad WIFI, hardware issues,..).
1. MQTT support to integrate machine in smart-home solutions and to easier extract details for graphing/alerting.
1. ACTION "SLEEPING" can be used to shutdown heater and turn the display off either after a period on user inactivity (default 120min) or by user request. Any user activity (controlActions, Hardware-Buttons, MQTT Actions, brew Detection,..) will wake up the maschine.
1. Added RemoteDebug over telnet so that we dont need USB to debug/tune pid anymore (https://github.com/JoaoLopesF/RemoteDebug). While using OTA updates you can remotely debug and update the software!
1. "Brew Ready" Detection implemented, which detects when the temperature has stabilized at setPoint. It can send an
   MQTT event or have hardware pin 15 triggered (which can be used to turn a LED on).
1. All heater power relevant settings are now set and given in percent (and not absolute output) and therefore better to understand
1. Support for customizable item collections to beautify display output (thanks to helge!)
1. Safetly toogle added to shutdown heater on sensor malfunction (TEMPSENSORRECOVERY)
1. Many useful functions to be used internally getAverageTemperature(), pastTemperatureChange() + updateTemperatureHistory())

## ATTENTION:
- This software is tested thoroughly with the pid-only hardware solution on Silvia 5e, and with a permanently run full-hardware solution on an 10 year old Silvia. Also a 10 year old Gaggia Classic is tested successfully. I am grateful for any further feedback. 
- Please monitor our machine's temperature closely the first few run times. The muti-state pid controller should never lead to temperatures greater than 5 degress above setpoint!

## Sample data log
- You can use the mqtt interface to export live data for monitoring purposes as can be seen in this [Grafana Dashboard](https://snapshot.raintank.io/dashboard/snapshot/lYe7XigrehSfVsvAWYifEwd2d5hNC0dl).

## Instructions on how to migrate from official rancilio to bleeding-edge
Installation is as explained on http://rancilio-pid.de/ but with following adapations:
1. Make screenshots of the official "Blynk App Dashboard" so that you can revert anytime.
1. Copy file userConfig.h.SAMPLE to userConfig.h and edit this file accordingly.
   - I propose to set MQTT_ENABLE=2 instead of blynk. But it is up to you.
     ```
     #define MQTT_ENABLE 2
     ```
1. Copy contents of folder ranciliopid\arduino-libs to your arduino sketchbook location (normally C:\Users\YOUR_NAME\Documents\Arduino)
   - Important: Open arduino IDE and configure the setting Tools->"lwip Variant" -> "1.4 High Bandwidth"
1. Flash and enjoy your espresso.
1. No tuning should be required normally. If you want/need to then use the method described below.

## Instructions on how to update to the latest version of bleeding-edge
1. Just overwrite all existing files with a newly released version.
2. Open your userConfig.h file, which had not been overwritten in previous step, and manually check (line by line!) that all updates to the new file userConfig.h.SAMPLE are reflected in your own userConfig.h. 
3. Compile, upload and enjoy!

## Remote Control APIs
- MQTT
  - Instructions can be found at https://github.com/medlor/bleeding-edge-ranciliopid/wiki/MQTT-Setup
- Blynk
  - Instructions can be found at https://github.com/medlor/bleeding-edge-ranciliopid/wiki/Blynk-Setup

## Tunings instructions
- Instructions can be found at https://github.com/medlor/bleeding-edge-ranciliopid/wiki/PID-Tuning

## Debugging Howto
- Instructions can be found at https://github.com/medlor/bleeding-edge-ranciliopid/wiki/Debugging-Howto

## How to use a simple LED as brewReady signal
- Instructions can be found at https://github.com/medlor/bleeding-edge-ranciliopid/wiki/How-to-use-a-simple-LED-as-brewReady-signal

## Instructions on how to create new icon collections
- Instructions can be found at https://github.com/medlor/bleeding-edge-ranciliopid/wiki/Instructions-on-how-to-create-new-icon-collections

## Changelog
- 3.2.2:
  - Refactor: Moved blynk code into own files. Thanks aschoelzhorn!
  - Refactor: Add a temperature sensor mock. Thanks aschoelzhorn!
  - Refactor: Replace state magic numbers with meaningful names. Thanks aschoelzhorn!
- 3.2.1:
  - Feature: Add support for SH1106 in SPI mode. Thanks piontec!
  - Feature: Add support for custom display pins (see DISPLAY_I2C_SCL, DISPLAY_I2C_SDA). Thanks piontec!
  - Feature: Distinguish triggertype for 3way-valve and pump. Thanks piontec!
  - userConfig.h Changes:
    - Added VALVE_TRIGGERTYPE, PUMP_TRIGGERTYPE (default HIGH. Previously called TRIGGERTYPE).
    - RENAMED pinRelayVentil to pinRelayValve.
    - RENAMED pinRelayPumpe to pinRelayPump.
    - Added DISPLAY_I2C_SCL, DISPLAY_I2C_SDA (default 255).
  - Fix: SSD1306 support should also work again. Thanks piontec!
  - Added IoTMQTTPanel-perfect_coffee_pid_v3.2.0_v1.json to support the "new" scale settings.
  - Fix: Changed BREWTIME_END_DETECTION to type "bool" to improve handling of improper configuration.
  - Upgrade ZACWire to v2.0.0. Thanks Adrian/lebuni.
  - Refactored GpioCheck accordingly.
  - Updated ESP boardmanager and library dependencies to latest official version:
    - Platformio:
      - esp32: Upgrade espressif32 from v3 to v5
      - FastLED to v3.5.0
      - esp8266: Upgrade espressif8266 from v2 to v4
      - esp8266: Update to latest uMQTTBroker
    - ArduinoIDE:
      - Upgrade Boardmanager from 
        - ESP32:v1.0.6 to v2.0.4 
        - ESP8266:v2.7.4 to 3.0.2
      - Important: Update all manually installed libs like RemoteDebug and ZACWire (see Wiki)!
- 3.2.0:
  - NEW FEATURE (ESP32 & ONLYPID=0): Add *SCALE* functionality for brewing based on a PR by [Hoondie](https://github.com/avolmensky) (Many thanks Hoondie!).
    - Rewrite of brew() function to supports stopping brewing by time or weight (see userConfig.h BREWTIME_END_DETECTION):    
    - Setup guide in [Wiki](https://github.com/medlor/bleeding-edge-ranciliopid/wiki/Scale-Setup)
      - 3D printer models:
        - [3D Scale Case made by Hoondie](https://www.thingiverse.com/thing:5226789) (Many thanks Hoondie!).
        - [PerfectCoffeePid Scale Case](https://github.com/medlor/bleeding-edge-ranciliopid/tree/master/3d-models/PerfectCoffeePid-Scale). 
    - Display shows actual weight while brewing and additionally brewStatistics/flowRate a couple of seconds after brew (all themes are supported).
    - Automatic tuning of target-weight offset to adapt to dipping effects after brew ends.
  - Refactoring und noticeable further features:
    - Implemented manual calibration feature:
      - Howto: 
        - Set ENABLE_CALIBRATION_MODE=1, start machine without any weight on the scale, 
          after around 20s put a known weight (which is set in SCALE_SENSOR_CALIBRATION_WEIGHT) on the scale.
          The LOG will show the calculated SCALE_SENSOR_CALIBRATION_FACTOR which has to be added to the userConfig.h .
    - Send brewStatistics to display, mqtt events and LOG: "Brew statistics: 37.36g in 29.78s (1.25g/s) with profile 1".
      - FlowRate also included in output.
      - Display shows brewStatistics after brew for a couple of seconds.
    - Use powerOff and powerUp to save energy and to reduce interrupt conflicts. HX711 is only enabled when brew is running. 
    - 3rd party library for the HX711_ADC by Olav Kallhovd (https://github.com/olkal/HX711_ADC) adapted to our needs (Many thanks Olav!):
      - [custom async implementation](https://github.com/medlor/HX711_ADC) while using ISR to ...
        - reduce processing delays
        - reduce competing interrupts between TSIC, display and scale
        - no TSIC errors!
    - Auto-tare weight (async; without delay) at start of brew.
    - After brew is finished weight is measured for an additional 2 seconds (to include drops after the pump has stopped).
    - If HX711 cannot be initialized on start of brew, fallback to time-based brewing.
    - New scale settings are now tied to "Profiles":
      - Set following settings in userConfig.h / MQTT / "CONTROLS_CONFIG" / Blynk (V64 := activeBrewTimeEndDetection , V65 := activeScaleSensorWeightSetPoint):
        - BREWTIME_END_DETECTIONX: if 0, stop by time else stop by weight 
        - SCALE_SENSOR_WEIGHT_SETPOINTX = weight when to stop brewing
    - Custom scaleConfigOverwrite.h
    - ArduinoIDE: Added library/HX711_ADC_fix which has to be manually copied to your arduino library path. (see wiki)
    - ArduinoIDE: Successfully compiles (in addition to Platformio).
  - ArduinoIDE: "library/RemoteDebug" minor compile fix.
  - ArduinoIDE: Add check to force esp32 board version 1.0.6.
  - brew() is only called at most every 1ms.
  - Fix: If you have an analog toggle configured for BREWING and dont turn it off before power-off, 
      then brewing will no longer automatically start on power-on.
  - userConfig.h Changes:
    - BREWTIMER_MODE renamed to BREWTIME_TIMER
    - Many new defines called SCALE_SENSOR_*
    - Added BREWTIME_END_DETECTIONX
    - Added SCALE_SENSOR_WEIGHT_SETPOINTX.
    - Added SCALE_SENSOR_CALIBRATION_WEIGHT, SCALE_SENSOR_CALIBRATION_FACTOR
    - Updated circuit-diagrams to add HX711.
- 3.1.1:
  - Fix: Coldstart working again.
- 3.1.0:
  - NEW FEATURE: Add support for up to 3 "*Profiles*", which can be used to quickly switch between different brew settings (eg. when different beans are used).
    - An individual profile comprises of following settings:
      - Brew Temperature / setPoint
      - Brew Time 
      - Preinfusion time and pause
      - Coldstart Temperature / startTemp
    - Profiles can be selected by any means, eg mqtt/blynk/menu/.. . The variable for this is "profile".
    - The selected profile is also shown on the display, see new setting ENABLE_PROFILE_STATUS.
    - ATTENTION: Following variable names has changed and therefore any mqtt dashboards and tools have to be adapted (eg IotMQTT/Grafana/nodered):
      - setPoint -> activeSetPoint
      - brewtime -> activeBrewTime
      - starttemp -> activeStartTemp
      - preinfusion -> activePreinfusion
      - preinfusionpause -> activePreinfusionPause
      - Hint: Changing the value of any of variables (eg using mqtt/blynk) will update the respective setting of the currently activated profile.
    - Blynk:
      - Added new virtual Pin V3 for "profile", which can be any number between 1 and 3.
      - Added [Blynk Showcase](https://github.com/medlor/bleeding-edge-ranciliopid/tree/master/blynk/blynk-showcase_v3.1.0.mp4).
  - NEW FEATURE: Initial support for MENU which can be used to control settings using the display-case with buttons:
    - Action "MENU" added.
    - Actions TEMP_INC and TEMP_DEC renamed to MENU_INC and MENU_DEC respectively.
    - Added new userConfig.h setting MENU_CONFIG which is used to configure menu settings.
    - Following menu operations are supported (can be extended on request):
      - SETPOINT, SETPOINTSTEAM, BREWTIME, PREINFUSION, PREINFUSION_PAUSE, PID_ON
  - NEW FEATURE: The calculation of the "total brew time" can be adjusted with the userConfig.h setting BREWTIMER_MODE.
     - 0: (default) brewtime-countdown is equal BREWTIMEX. 
     - 1: brewtime-countdown is BREWTIMEX + PREINFUSIONX + PREINFUSION_PAUSEX
  - userConfig.h Changes:
    - Added:
      - ENABLE_PROFILE_STATUS
      - Renamed SETPOINT to SETPOINT1
      - Renamed BREWTIME to SETPOINT1
      - Renamed PREINFUSION to PREINFUSION1
      - Renamed PREINFUSION_PAUSE to PREINFUSION_PAUSE1
      - TEMPSENSOR (default=2)
      - SETPOINT2, SETPOINT3
      - STARTTEMP2, STARTTEMP3
      - PREINFUSION2, PREINFUSION3
      - PREINFUSION_PAUSE2, PREINFUSION_PAUSE3
      - BREWTIMER_MODE
  - Support for Thermistor K-type with MAX6675 (Thanks to aschoelzhorn for the PR and TeraK, sailhobie for support).
    - TSIC3xx is still strongly recommended to use!
  - Updated IoTMQTT Dashboard [IoTMQTTPanel-perfect_coffee_pid_v3.1.0_v1.json](https://github.com/medlor/bleeding-edge-ranciliopid/tree/master/IoTMQTTPanel/IoTMQTTPanel-perfect_coffee_pid_v3.1.0_v1.json) to include Profile and Action support. [Showcase](https://github.com/medlor/bleeding-edge-ranciliopid/tree/master/IoTMQTTPanel/IoTMQTT-showcase_v3.1.0.mp4).
  - Refactor and fix eeprom stuff.
  - Refactor all variables from double to float.
  - Make blynk optional on compilation.
  - Removed burst feature.
  - Fixes:
    - Fix blynk "heater output" not being shown.
    - Fix float compare function.  
- 3.0.0:
  - Project is getting renamed to "Perfect Coffee PID".
  - Improve temperature readings to reduce undesireable actions of the PID controller:
    - Detect outliers near setpoint and fix them automatically.
    - Read temperature every 100ms (previous 1s) and save it to TemperatureHistory for later use.
    - Implement average calculations on all temperature functions to stabilize temp readings.
    - Temporary use custom ZACwire library, which offer:
      - bitwindow auto-tuning right from the start.
      - directMode to improve outlier detection and flapping temperature in seldom cases (esp32).
      - Important: You have to [manually install](https://github.com/medlor/bleeding-edge-ranciliopid/wiki/How-to-compile-on-Arduino-and--Platformio) library/ZACwire_for_TSic (only if you are using Arduino IDE).
  - Add full support for platformio, code cleanup and fix compile warnings.
    - Force espressif8266 to Version 2 (until external libs are adapted)
  - Arduino IDE: Remove some libaries which from now on have to be installed manually, see [Wiki](https://github.com/medlor/bleeding-edge-ranciliopid/wiki/How-to-compile-on-Arduino-Platformio).
  - Performance improvement: 
    - controlActions are now using interrupts with an optional debounce feature (instead of polling each gpio).
  - Update ESP32 schematic to reflect recommendation to use 5v (instead of 3.3v) for the TSIC.
  - Fix: 
    - DISPLAY_TEXT_STATE is working on exp8266.
    - controllerActions: Improve debouncing of analog(ADC) and digital buttons/switches.
    - Improve steaming cycle-times.
  - Remove support for DS19B20 temperature sensors.
  - Improvements in error logging.
  - UserConfig Changes:
    - Breaking Change:
      - renamed DISPLAY to DISPLAY_HARDWARE
    - Removed:
      - TEMPSENSOR
    - Added:
      - DEBOUNCE_ANALOG_GPIO / DEBOUNCE_DIGITAL_GPIO
    - Changes:
      - recommended defaults
- 2.8.0:
  - Feature: Add support to detect low water using a VL53L0X Time of Flight distance sensor. see [Howto](https://github.com/medlor/bleeding-edge-ranciliopid/wiki/Water-level-measurement-using-VL53L0X-(Time-of-Flight-distance-sensor))
  - Helge supplied a 3d printing model for rancilio silvia's water tank lid which can be used with the VL53L0X sensor. (thanks Helge!)
  - Update ZACWire to v1.3.2. (Thanks Adrian)
  - UserConfig change: 
    - Renamed DEBUG_FORCE_GPIO_CHECK to ENABLE_CALIBRATION_MODE.
  - Fix: Show general logo when MASCHINE_TYPE is set to unknown.
- 2.7.1:
  - Added PR for automatic cleaning mode (Thanks to urbantrout/Harald).
    - Adaptions:
      - New cleaning icon: ["washing hand"](https://thenounproject.com/matfine/collection/cleaning-icon/?i=2749704) by [Mat fine](https://thenounproject.com/matfine) from [the Noun Project](https://thenounproject.com/matfine/collection/cleaning-icon/?i=2749704) licensed under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/legalcode)
      - New Blynk VPins (read-only/pull) defined: see [Blynk-Setup](https://github.com/medlor/bleeding-edge-ranciliopid/wiki/Blynk-Setup)
      - New userConfig Settings: 
        - CLEANING_CYCLES, CLEANING_INTERVAL, CLEANING_PAUSE.
        - Set CLEANING_ENABLE_AUTOMATIC=1 to use automatic cleaning program instead of manual modus.
  - Feature: (Silvia5E): ENABLE_POWER_OFF_COUNTDOWN timer (in minutes) is available via mqtt("powerOffTimer")/blynk(V45).
  - Blynk is fully supporting actionController. Therefore any action can be called via blynk. (see [Blynk-Setup](https://github.com/medlor/bleeding-edge-ranciliopid/wiki/Blynk-Setup))
  - Added new icon-collection "static" which just shows a non-moving logo.
  - Added userConfig DISPLAY_TEXT_STATE which outputs few status information (Please wait, Ready, Turned off, Cleaning, ..) in the display (supported by all icon-collections).
    - Text is not displayed when screensaver is running.
  - Fix: Waking up after SLEEPING action re-enables cold-start autotuning.
  - Fix: Blynk sometimes crashes when controlActions are changed.
  - Fix: SLEEPING action will turn off display even if ENABLE_SCREEN_SAVER=0.
  - Fix: DISPLAY_TEXT_STATE is not shown during BREWING.
  - Update ZACwire-Library to v1.3.1. and 1.3.0 (beta). (Thanks again Adrian :-) )
- 2.7.0:
  - Feature: FULLY SUPPORT ESP32 with CPU pinning for improved performance.
    - Unfortuately there is no mqtt-server broker library available for ESP32. (MQTT_ENABLE must not be 2 or disabled on ESP32)
    - Display function is pinned to secondary cpu to further improve performance. No more tearing on screen refresh.
  - [ZACwire-Library](https://github.com/lebuni/ZACwire-Library) upgrade to stable release v1.3.0 which supports ESP32 and pinning to secondary cpu. Thanks Adrian!
  - Feature: "FULL CONTROL" hardware modification is fully supported (in addition to the previous "PidOnly" and "standard" modification)!
    - FullControl have all(!) functionality directly controlled by the esp (eg. without the need to manually switch any buttons).
    - Instead of having the hardware-switches control brew/hotwater/steaming and have the esp just "cut" the wire, the FullControl modification detects button presses directly and translates them to any custom functionality (using ControlActions, MultiToggle and optionally GpioActions). This allows maximum flexibility.
  - Feature: The "MultiToggle" feature triggers any ControlAction based on multiple simultaneous hardware-button states (eg. if hotwater+steaming button is pressed then start the action cleaning).
    - This allows to use a total of six ControlActions just by using the three rancilio's hardware-switches and reduces the need for external control (mqtt, blynk, resistor-circuit, ..).
    - New UserConfig variables: MULTI_TOGGLE_?_ACTION and MULTI_TOGGLE_?_GPIOS.
  - Feature: GpioAction: If actions are activated, specific GPIOs can be triggered also. This can be used eg. to light up LEDs in Rancilio's [custom modified hardware-switches](https://voir.pt/2017/12/30/rancilio-silvia-bezugsschalter-als-softwareschalter-umruesten/).
    - userConfig.h: ENABLE_GPIO_ACTION / pinBrewAction / pinHotwaterAction / pinSteamingAction
    - CLEANING action will turn on the brew/hotwater/steaming gpios to make this state easily visible even when no display is used.
    - During PowerOn GpioActions and HardwareLed are enabled for a couple of seconds.
  - Feature: Added new state "SLEEPING", which shutdowns heater and turns display off either after a period on user inactivity (default 120min) or by user request using the new ControlAction "SLEEPING". Any user activity  (controlActions, Hardware-Buttons, MQTT Actions, brew Detection,..) will wake up the maschine.
    - New userConfig setting: userConfig HEATER_INACTIVITY_TIMER (0 to disable)
  - Feature: (!OnlyPid|Full Control) Added a CLEANING mode which sets pre-infusion pause to 0seconds and disables auto-tuning for as long as mode is active. Use "CLEANING" ControlAction or activate steaming to restore normal operation.
  - Feature: You can now set INPUT/INPUT_PULLDOWN/INPUT_PULLUP in CONTROLS_CONFIG. 
  - Feature: (ENABLE_HARDWARE_LED=2) New settings ENABLE_HARDWARE_LED_RGB_ON/ENABLE_HARDWARE_LED_RGB_OFF to set specific colors.
  - Breaking changes:
    - CONTROLS_CONFIG format changes to support new INPUT/INPUT_PULLDOWN/INPUT_PULLUP settings.
  - CHANGE: (DISPLAY=1) HardwareLed is not turned on when screensaver is running and ENABLE_HARDWARE_LED_OFF_WHEN_SCREENSAVER=1 (even if brew_ready state is reached).
  - Fix: Setting setPointSteam via mqtt did not work but did overwrite aggoTv instead. You should update to this version when using mqtt.
  - Fix: No one-time heater flapping if maschine is waked up from sleep or pidOff.
  - Fix: HardwareLed is only turned on, when temperature > steam_ready_temp AND steaming action is active.
  - Fix: (DISPLAY=1) HardwareLed is not turned on when screensaver is running (even if brew_ready state is reached).
  - Fix: Minor fixes here and there.
  - PubSubClient updated to version 2.8.
  - Update userConfig.h.SAMPLE (Thanks Helge)
  - Fix typos (Thanks urbantrout)
  - (ESP32) Added two new [circuit-diagrams](https://github.com/medlor/bleeding-edge-ranciliopid/tree/master/circuit-diagrams) which show the complete wiring in "full-control" upgrade.
  - Added several 3d models to print:
    - display-case-with-buttons
    - dual-ssr-relay-case
    - pcb-case (eg. to be used for esp32 on a pcb)
  - New images here and there.
  - Quick documentation on how to setup ESP32 can be found in the [wiki](https://github.com/medlor/bleeding-edge-ranciliopid/wiki/ESP32-Setup).
  - Thanks to Tobias for his helpful blog posts at [https://voir.pt](https://voir.pt/2017/12/30/rancilio-silvia-bezugsschalter-als-softwareschalter-umruesten/)
- 2.6.1:
  - Feature: Hardware-Led in addition to simple LEDs also support WS2812b LED (stripes) (MANY THANKS P1Rebo for the PR)
- 2.6.0:
  - Merged PR by finnito which changed/extended: (MANY THANKS FINNITO)
    - Added support for ECM espresso machine including logo.
    - Added/Extended support for special functions (steam, hotwater, brew).
    - Added custom steam functionality.
    - Extended initial support to call special functions based on Pin A0 (analog levels) which can be used by eg. hardware buttons.
    - Added new state to state machine (6=steam).
  - Refactored/Extended finnito's code to integrate our new "actionController":
    - fully dynamic mapping of any(!) available analog/digital gpio port to custom functions, which are currently:
      - BREWING   := start brewing
      - STEAMING  := automatically manages heat up machine for steaming
      - HOTWATER  := start pouring hotwater
      - HEATER    := activate heater (not yet)
      - PUMP      := activate pump (not yet)
      - VALVE     := activate Valve  (not yet)
      - CLEANING  := activate cleaning mode (not yet)
      - TEMP_INC  := increase setPoint (not yet)
      - TEMP_DEC  := decrease setPoint (not yet)
    - while also supporting the switch types: toggles (eg switches) and triggers (eg push buttons)
    - added MQTT Support to control actions using topics ../actions/<ACTION> with supported payloads of 0|1|-1 (off|on|switch)
      Example: "custom/Küche.Rancilio2/actions/STEAMING"
  - Feature: Set userConfig.h ROTATE_DISPLAY to rotate display 180 degree.
  - Feature: BrewReadyLed (now called HardwareLed) configured with ENABLE_HARDWARE_LED also lights up when temperature>steamTemp or >STEAM_READY_TEMP.
  - Feature: OnlyPid=0: Disable pre-infusion and pause if userConfig settings are set to 0.
  - Feature: OnlyPid=1: BrewDetection can also be triggered by hardware using actionControl "BREWING".
  - Feature: New userConfig setting SETPOINT_STEAM is also setable via [mqtt](https://github.com/medlor/bleeding-edge-ranciliopid/wiki/MQTT-Setup)/[blynk](https://github.com/medlor/bleeding-edge-ranciliopid/wiki/Blynk-Setup).
  - Feature: hardwareLed will glow for a few seconds when the machine starts up. This helps determine a not-starting node.
  - Feature: Additional safe-guard to temporary disable heater if temperature is 10 degree above active setPoint.
  - Important FIX: mqtt interface for STEADYPOWER_OFFSET_TIME working again. This setting is currenly wrongly set and need to be re-set for auto-tuning to work!
  - Fix: EMERGENCY_TEMP setting disables pid when temperature is > and re-enables when < setPoint. Please check your setting to be reasonable high (eg. Rancilio 135 degree). Should at least be higher than SETPOINT_STEAM.
  - Feature/Fix: New setting STEAM_READY_TEMP to show our special steaming-icon only when temperature is above.
    - There is support for special "in-steaming-state" icons, but I am in no mood to paint them yet. Feel free. 
  - BREWDETECTION_POWER is now in active right after a brew is detected and not when temp <1.5degree below setpoint.
  - Attention: Major userConfig adaptions. Create a backup!
    - BREAKING CHANGE: 
      - OnlyPid=1: Software based brewDetection has to set with "2" (and not "1"!!) in userConfig BREWDETECTION.
    - Added:
      - SETPOINT_STEAM
      - STEAM_READY_TEMP
      - CONTROLS_CONFIG
      - DEBUG_FORCE_GPIO_CHECK
      - ROTATE_DISPLAY
    - Removed:
      - pinBrewButton (replaced by CONTROLS_CONFIG)
      - EMERGENCY_ICON
      - OTAHOST (use HOSTNAME instead)
      - ENABLE_USER_MENU
    - Changed:
      - Default of userConfig EMERGENCY_TEMP from 125 to 128.
      - Default of userConfig TEMPSENSORRECOVERY from 1 to 0.
  - Sample Config for IoTMQTTPanel: IoTMQTTPanel-rancilio_v2.6.0_v1.json
  - Update ZACwire-Library to v1.1.3.
- 2.4.2 master:
  - Bugfix: Revert changes in regard to compiler warnings added in 2.4.1.
  - Bugfix (MQTT_ENABLE=1): Auto-Tuning settings are persisted across restarts.
  - Bugfix: steadyPowerOffsetTime is configurable via mqtt.
  - Bugfix: aggoTn is now correctly published via mqtt.
  - Added new circuit-diagrams (Thanks Tempcookies + Helge).
  - MQTT code refactorings.
- 2.4.1 master:
  - It fixes some compiler errors and warnings I was seeing on my system. (Thanks nils-werner)
  - It contains a small platformio.ini file for Platformio support. (Thanks nils-werner)
- 2.4.0 master:
  - Special winter theme added. Activate with ICON_COLLECTION=2 in userConfig.h.
  - Added support for a customizable screen-saver (#define ENABLE_SCREEN_SAVER)
  - Overwrite Version info at boot time via OVERWRITE_VERSION_DISPLAY_TEXT in userConfig.h.
  - Added power-off countdown to hint the user when the machine is going to shutdown (ENABLE_POWER_OFF_COUNTDOWN) (Silvia 5E)
  - Power-off countdown starts 5min before shutdown.
  - Bugfix: Fix brew detection (OnlyPid)
  - Bugfix: End of BrewTimer correctly displayed (OnlyPid)
  - Bugfix: No power-off countdown when brewing.
  - Move display stuff into separate files
  - alpha code: Intitial support for an user-menu (ENABLE_USER_MENU/pinBrewButton)
  - alpha code: Initial infos about how to add a case with hardware-buttons to control basic settings directly at machine.
- 2.3.0 master:
  - PID is completely auto-tuned and should work flawlessly after a few starts. No need to configure PID any longer. 
  - Bleeding-Edge settings can be configured remotely without requiring a running service (blynk, mqtt-server) running in your network or internet. (No extra raspi required)
    You can now freely choose which service you want to use:
    1. Standalone without any network connectivity
    2. MQTT Server (runs on bleeding-edge arduino) to not depend on a remotely running service
    3. MQTT Client to connect to an mqtt-services (eg used by smart-home software)
  - MQTT fully implemented so you can publish and subscribe to configure settings and monitor states.
  - THANKS: Added Adrian's library to efficiently read TSIC values (https://github.com/lebuni/ZACwire-Library)
  - ATTENTION: New default values in userConfig.h. 
    - It is recommended to intially use default values for STEADYPOWER, STEADYPOWER_OFFSET_TIME, STEADYPOWER_OFFSET, STARTTEMP, BREWDETECTION_POWER, BREWDETECTION_SENSITIVITY. Additionally BREWTIME is from now on also used in ONLYPID=1.
    - GPIO Pin Mapping moved to userConfig.h.
  - ATTENTION: Installation process changed. Libs must be installed as described in "Instructions on how to migrate from official rancilio to bleeding-edge 
    - Copy contents of folder ranciliopid\arduino-libs to your arduino sketchbook location (normally C:\Users\YOUR_NAME\Documents\Arduino)
  - Improve PID:
    - Auto-tuning for starttemp is implemented. No need to adapt the STARTTEMP accordingly when SETPOINT is modified
    - steadyPowerOffset is gradually decreased over time to better compensate the warm up of machine.
    - PID monitors and tunes steadyPowerOffset.
    - One time PID manipulation logic added.
    - PID's I parameter filter is reduced in certain situations more strictly.
  - BrewDetection optimized:
    - (ONLYPID=1) ATTENTION: The BREWTIME setting defines the time-frame of your "normal" brew (in seconds). 
       When a brew is detected, the brew_timer starts and runs until BREWTIME is reached. During that time
       the heater is heating with BREWDETECTION_POWER.
    - BREWDETECTION_POWER behaviour changed: 
      (ONLY_PID=0) The power is applied to heater during the complete brew process. 
      (ONLY_PID=1) The power is applied until the brew_timer is larger than BREWTIME or the current temperature is 1.5 Celcius below setpoint.
    - (ONLYPID=1) BREWDETECTION_WAIT setting added: After a brew is started the software based BrewDetection is disabled for this number of seconds to prevent the detection of another brew when flushing water.
    - (ONLYPID=1) Software BrewDetection estimates the starttime of a brew, so that the brew counter in display is correct. 
  - Debug Logs Improvements:
    - Add logs for brewReadyStatistic
    - When connecting by telnet/blynk the hardware- and software configuration is printed
  - Documentation Updates:
    - Enabled wiki at https://github.com/medlor/bleeding-edge-ranciliopid/wiki/
      - eg. Howto "Setup MQTT": https://github.com/medlor/bleeding-edge-ranciliopid/wiki/MQTT-Setup
    - Added link to sample daily Grafana dashboard.
  - Fix: userConfig.h: AUTH renamed to BLYNKAUTH
  - Fix: No starttemp tuning when when machine is already warm.
  - Fix: burstPower working again.
  - Fix: If PID is manually disabled, heater utilization is correctly reported as 0%.
  - Fix: Heater overextending handling working as intended.
  - Fix: Potential crash when toogling pid on/off
  - Fix: Save pidON in eeprom.
  - MQTT_ENABLE=1: Configuration retained in mqtt topics are now used on startup.
  - Code cleanup and refactorings
  - Trigger brewReady when temperature is stable for 60sec (prev: 40s).
- 2.2.0_master:
  - Display functionality improved:
    - Replaced display lib Adafruit_SSD1306.h with U8G2. Direct support for SH1106_128X64 and SSD1306_128X64 via userConfig.
    - Completly new display widgets which show informations according to active PID state.
    - Support for icon collections to easily customize/share display widgets. Icon collection "simple" and "smiley" included.
    - Support for simple icon animations.
    - Service Status Icons are displayed if enabled in userConfig.
    - New DEFINES in userConfig.h. Update your config.
  - Improvement: Blynk On/Off button re-inits PID state similar to when machine power is turned on.
  - Fix: If DISABLE_SERVICES_ON_STARTUP_ERRORS=1 then WIFI reconnect attempts are also prevented.
  - Remove define EMERGENCY_TEXT.
  - Fix: PID State "brewing" is correctly detected when ONLYPID=0. (Thanks Helge)
- 2.1.0_master:
  - Networking:
    - Huge improvements in handling unstable WIFI networks and mqtt/blynk service unavailabilities.
    - You can disable/enable WIFI, MQTT or Blynk in userConfig.h and stil have a flawlessly working PID controller. Blynk is no longer an hard requirement!
    - Offline Modus is fixed and enhanced. If userConfig.h's FORCE_OFFLINE is enabled, then PID fully is working without networking. Ideal in situations when there is no connectivity or you dont want to rely on it.
    - Fix of EEPROM functionality: PID settings are correctly saved in EERPOM and correctly used if there are WIFI issues.
    - Instead of using dynamic IPs (over DHCPd) you have the option to set a static IP.
    - If blynk or mqtt is not working during startup, do not retry the connection periodically (configurable by userconfig.h DISABLE_SERVICES_ON_STARTUP_ERRORS)
  - New PID Variable "BREWDETECTION_POWER" introduced which defines the heater power during brewing.
  - Complete rewrite of "TSIC sensor read" based on the excellent ISR code by Adrian. (Thanks Adrian!)
    - Optimized "TSIC sensor read" to further increase performance.
    - Currently it takes <4ms to collect sensor data instead of the previous 78ms).
    - Fix: "sensor errors" do not occur anymore.
  - Performance/Stability:
    - Some system libs are optimized in performance and stability (src/ folder).
    - Remove all unneeded external libraries which are installed in system's arduino search path.
    - Code Tunings all over the place to increase performance and therefor stability.
    - Overall stability better by honoring critical processes.
  - Debuglogs can also be accessed via browser (see documentation).
  - PID calculations are moved from ISR to loop(). This improves stability even further.
  - Fix: Brew detection optimized.
  - Safetly feature: Done start brewing if the brew-button is switched "on" on startup
  - Fix: After power on, the 5 second wait time until heater starts is removed.
  - Library path adapted to support Arduino under Linux.
- 2.0.3_beta2:
  - Wifi disconnects handled better.
  - Implement blynk reconnect exponential backoff.
  - Set blynk reconnection timeout (3sec).
  - Added debug messages in ciritical paths.
- 2.0.2_master:
  - stable release
  - updated docs
- 2.0.2_beta2:
  - removed 8x8 display support
  - Code cleanup
- 2.0.2_beta1:
  - Major improvements in display related stuff (eg new icons,..) (thanks helge)
  - Restructure folders (thanks helge)
- 2.0.1_beta8:
  - ISR performance optimised when debug is active (spend time reduced from 0.8ms to 0.15ms).
  - Move DEBUGMODE to config.h.
- 2.0.1_beta7:
  - Emergency Logo can be replaced by a nice "milk steam" logo (EMERGENCY_ICON). (Thanks helge for the icon)
  - Regular status display is beautified and adapted to normal users needs. (Thanks helge for the code)
- 2.0.1_beta6:
  - Fix: Reducing "temperature sensor reading" errors.
  - Fix: steadyPowerOffset_Time is now correctly configurable via blynk.
  - Fix: Hardware switch sensitivity ("brewswitch") is increased from 1000 to 700.
  - Improved: mySteadyPower failure detections.
  - Code Cleanup + Refactorings.
- 2.0.1_beta5:
  - RemoteDebug inactivity time requires a version not yet in arduino library manager. Therefore a workaround is document in the meantime.
  - Pre-Infusion variables are now configurable in config.h: PREINFUSION, PREINFUSION_PAUSE, BREWTIME
  - Emergency temperature theshold is now configurable in config.h: EMERGENCY_TEMP
  - BrewDetection sensitivity is now configurable in config.h: BREWDETECTION_SENSITIVITY (renamed from brewboarder)
- 2.0.1_beta4:
  - Limit auto-tuning of steadyPower to reduce overly increased values.
  - Fix bug in brew().bezugsZeit calculation.
  - Renamed outputK to outputI.
  - Explanation of PID log line added.
- 2.0.1_beta3:
  - Pre-Infusion Values are now in seconds (previously milliseconds).  
    !! ATTENTION: Therefore you HAVE to change following values via blynk: brewtime, preinfusion, preinfusionpause !!
  - Reduce overhead of preinfusion functionality by having it run just every 50ms (and not every ms).
  - Add support for custom emergency text in display when temp >120 degree. (see EMERGENCY_TEXT in config.h.SAMPLE)
  - Emergency temperature threshold can now be set by variable emergency_temperature.
  - RemoteDebug inactivity time increased from 10min to 30min.
  - Add missing line-break in brew() debug logging.
- 2.0.1_beta2:
  - Better Installation Guide
  - Fix brew() function
  - Support virtual BrewReady LED in blynk
  - Improve pastChange() in Pid.compute(). (default Kd value has to be doubled)
  - Improve Auto-tuning.
  - Default PID Value Tunings
- 2.0.1_beta:
  - ATTENTION: EEPROM has changed. Therefore you have to connect to blynk at least once, and manually set correct settings in blynk app (see screenshots for default values).
  - New PID Controller "Multi-state PID with steadyPower (Bias)"
  - "Brew Ready" Detection implemented, which detects when the temperature has stabilized at setPoint. It can send an
    MQTT event or have hardware pin 15 triggered (which can be used to turn a LED on).
  - Because deadtime of Silvia5E is around 45seconds PID.compute() runs every 5 seconds (previous 1sec) and reduce header on/off switches by factor 5
  - Temperature polling is also now set to once every second (previous 400ms)
  - Code refactoring/cleanup and fixes of bugs.
  - BurstShot feature added to temporary overwrite PID controls (useful mainly for tests)
  - Added RemoteDebug over telnet so that we dont need USB to debug/tune pid anymore (https://github.com/JoaoLopesF/RemoteDebug)
    - Just "$ telnet rancilio_ip 23"
  - Fix: Recover EmergencyStop when temperature poll in setup() fails
  - EmergencyStop state logging improved
- 1.9.8h_alpha:
  - Feature: Implemented special 3 step cold-start mechanism. See special tuning instructions above.
- 1.9.8g:
  - Improvement: Broken temperature is detected when temp has increased more than >5 degrees (previous 25 degrees) in the last 0.4 seconds.
  - Improvement: Better brewReady detection by waiting for stable temperature within a longer time window (from 6 to 14secs).
  - TemperatureHistory increased from 6 seconds to 30seconds.
- 1.9.8f:
  - Added missing config.h.SAMPLE variables.
  - Added some more mqtt events.
- 1.9.8e:
  - Removed movAvg() due to several issues:
    - This is no moving avg, but something different (but it is working for "stable" temperature curves).
    - readIndex=0 is not used.
    - movingAvg does not compares currentValue with previousValue.
    - Faktor *100 makes no sense (?).
    - initializing methods/firstreading is refactored.
  - movAvg() is replaced by pastTemperatureChange() (+ updateTemperatureHistory())
  - Change order of brew-detection in main loop() to not confuse bPid.compute() due to flapping setTunings()
  - Feature: isBrewReady() to determine if temperature is stable to start brewing
  - Bugfix: Replace abs() with fabs().
- 1.9.8d:
- 1.9.8c:
  - Add more mqtt updates: kp,ki,kd, coldstart
  - Safe-guard: be sure pid internals are cleared when pid is dis/enabled.
- 1.9.8b:
  - Feature: Add support for MQTT.
  - Safeguard: HeaterPreventFlapping must never be > windowSize.
- 1.9.7:
  - Fix: If temperature sensor detects more than 150Celcius then it must be an error.
  - Fix: Typos / explanations.
  - Fix: Race condition when code is blocked for some time
  - Fix: Added initial temp poll to prevent error on start.
  - Feature: Detect missing temperature polls.
  - Feature: Add configurable BLYNKPORT.
  - Feature: Wifi hostname configuable.
  - New feature: Safe-guard: Stop heating forever if sensor is flapping!
  - Fix: Pid.Compute() is now in sync with isrCounter and is not loosing one tick each second. Heater flapping is reduced.


# Credits & Special Thanks
To the great work of the rancilio-pid.de team.
Also to the nice people in our rancilio discord channel and the ones who contribute and give very much appreciated feedback like 
- helge/Helgelino
- Adrian 
- Finnito
- PedroR/P1Rebo
- urbantrout
- nils-werner

!! Thank you so much for the tasty cup of coffee I enjoy each day !!  

## Also special thanks to the great icon artwork
- ["washing hand"](https://thenounproject.com/matfine/collection/cleaning-icon/?i=2749704) by [Mat fine](https://thenounproject.com/matfine) from [the Noun Project](https://thenounproject.com/matfine/collection/cleaning-icon/?i=2749704) licensed under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/legalcode)
- ["menu"](https://thenounproject.com/icon/menu-2943760/) by [Mat fine](https://thenounproject.com/matfine) from [the Noun Project](https://thenounproject.com/icon/tasks-2943947/) licensed under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/legalcode)
- ["Glückliche Gesichter Emoticons"](https://pixabay.com/de/illustrations/gl%C3%BCckliche-gesichter-emoticons-5049116/) by [Annalise Batista](https://pixabay.com/de/users/annaliseart-7089643/) licensed under [Pixabay license](https://pixabay.com/de/service/terms/#license)
  


# Disclaimer
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
