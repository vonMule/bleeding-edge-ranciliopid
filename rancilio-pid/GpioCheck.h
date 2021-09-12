/********************************************************
 * BLEEDING EDGE RANCILIO-PID.
 * https://github.com/medlor/bleeding-edge-ranciliopid
 * 
 * Based on code of ZACwire-Library written by lebuni/adrian
 * https://github.com/lebuni/ZACwire-Library
 *****************************************************/

#ifndef _gpiocheck_H
#define _gpiocheck_H

#include "Arduino.h"
#include "rancilio-pid.h"

extern int convertPortModeToDefine(char* portMode);


template <uint8_t pin>
class GpioCheck {

	public:
		GpioCheck(char* portMode, unsigned int debounceTime = 30) {
			_portMode = portMode;
			counter = 0;
			gpioTime = 0;
			_debounceTime = debounceTime ; //ms
		}

		bool begin() {
			counter = 0;
			gpioTime = 0;
			isrPin = digitalPinToInterrupt(pin);
			//DEBUG_print("pin=%u isrPin=%d\n", pin, isrPin);
			if (isrPin == 255 || isrPin == -1) return false;
			#ifdef ESP32
			//DEBUG_print("%u: xTaskCreatePinnedToCore()\n", pin);
			xTaskCreatePinnedToCore(attachISR_ESP32_2,"attachISR_ESP32_2",4000,NULL,1,NULL,1); //freeRTOS
			#else
			pinMode(pin, convertPortModeToDefine(_portMode));
			attachInterrupt(isrPin, read, CHANGE);
			#endif
			delay(110);
			return true;
		}
	  
		byte getCounter() {
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

		unsigned int getGpioTime() {
			return gpioTime;
		}

		void end() {
			detachInterrupt(digitalPinToInterrupt(pin));
		}

	private:

		#ifdef ESP32
		static void attachISR_ESP32_2(void *arg){					//attach ISR in freeRTOS because arduino can't attachInterrupt() inside of template class
			//DEBUG_print("attachISR_ESP32_2()\n");
			gpio_config_t gpioConfig;
			gpioConfig.pin_bit_mask = ((uint64_t)(((uint64_t)1)<<pin));
			gpioConfig.mode         = GPIO_MODE_INPUT;
			gpioConfig.pull_up_en   = convertPortModeToDefine(_portMode) == INPUT_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
			gpioConfig.pull_down_en = convertPortModeToDefine(_portMode) == INPUT_PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
			gpioConfig.intr_type    = GPIO_INTR_ANYEDGE;
			ESP_ERROR_CHECK(gpio_config(& gpioConfig));
			gpio_install_isr_service(0);
			gpio_isr_handler_add((gpio_num_t)pin, read, NULL);
			vTaskDelete(NULL);
		}
		static void IRAM_ATTR read(void *arg) {
		#elif defined(ESP8266)
		static void ICACHE_RAM_ATTR read() {
		#else
		static void read() {
		#endif
			gpioTime = millis();  //estimated time of last gpio change
			if (counter >= 253) { counter=1; }
			else { counter++; }
		}

		
		unsigned int _debounceTime;
		static char* _portMode;
		static byte isrPin;
		static volatile unsigned char counter;
    	static volatile unsigned long gpioTime;  //TODO volatile too big
};

template<uint8_t pin>
volatile unsigned long GpioCheck<pin>::gpioTime;
template<uint8_t pin>
byte GpioCheck<pin>::isrPin = 255;
template<uint8_t pin>
volatile unsigned char GpioCheck<pin>::counter;
template<uint8_t pin>
char* GpioCheck<pin>::_portMode;


// -- 
char* createGpioCheck(uint8_t gpio, char* portMode, unsigned int debounceTime = DEBOUNCE_DIGITAL_GPIO);
void beginGpioCheck(uint8_t gpio, char* GpioCheckInstance);
unsigned char counterGpioCheck(uint8_t gpio, char* GpioCheckInstance);
#endif