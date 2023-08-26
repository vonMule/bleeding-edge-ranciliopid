#include "GpioCheck.h"
#include "rancilio-debug.h"

GpioCheck::GpioCheck(uint8_t inputPin, char* portMode, unsigned int debounceTime) {
			counter = 0;
			gpioTime = 0;
			_pin = inputPin;
			_portMode = portMode;
			_debounceTime = debounceTime ; //ms
		}

bool GpioCheck::begin() {
			counter = 0;
			gpioTime = 0;
			pinMode(_pin, convertPortModeToDefine(_portMode));
			uint8_t isrPin = digitalPinToInterrupt(_pin);
			//DEBUG_print("pin=%u isrPin=%d\n", pin, isrPin);
			if (isrPin == 255 || isrPin == -1) return false;
			#if defined(ESP32) || defined(ESP8266)
				attachInterruptArg(isrPin, isrHandler, this, CHANGE);
			#else
				attachInterrupt(isrPin, read, CHANGE);
			#endif
			return true;
		}

void GpioCheck::end() {
	detachInterrupt(digitalPinToInterrupt(_pin));
}

void GpioCheck::isrHandler(void* ptr) {
    static_cast<GpioCheck*>(ptr)->read();
}

void GpioCheck::read() {
	gpioTime = millis();  //estimated time of last gpio change
	if (counter >= 253) { counter=1; }
	else { counter++; }
}

byte GpioCheck::getCounter() {
			if (counter >0 && _debounceTime >0) {
				unsigned long diffTime = millis() - gpioTime;
				//DEBUG_print("getCounter(%d): %u gpioTime=%lu diffTime=%lu ms\n", pin, counter, gpioTime, diffTime);
				if (diffTime >= _debounceTime) {
					byte counter_temp = counter;
					counter = 0;
					gpioTime = 0;
					return counter_temp;
				} else {
					return 0;
				}
			}
			return counter;
		}

unsigned int GpioCheck::getGpioTime() {
	return gpioTime;
}
