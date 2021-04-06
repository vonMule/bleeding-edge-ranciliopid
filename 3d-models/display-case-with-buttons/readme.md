
!!!!!!!!!!!!!!!
ALPHA STATUS
!!!!!!!!!!!!!!!

## Scenario:
- 3D printed case (see stl, gcodes)
- housing is used for 1.3" TFT and supports 3 hardware buttons.
- detection of button presses by measuring volt changes on A0 (ADC0) port.
- buttons can be used for simple tasks (like changing temperature, enable stream temp, clean mode, ...) <-- not yet implemented


### Hardware setup:
  - check pictures in subfolder to connect display, Ground, 3V3 and A0 as seen in the circuit.png.


### Software setup:
  - ```
    #define ENABLE_USER_MENU  1               // 0 = off | 1 = on (only for ONLYPID=1) USE GPIO A0 (ADC0) PIN to detect the hardware buttons
    ```
  - adapt checkControlButtons() <- for now until this is fully configured via defines.

