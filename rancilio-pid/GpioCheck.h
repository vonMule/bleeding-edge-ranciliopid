/********************************************************
 * Perfect Coffee PID
 * https://github.com/medlor/bleeding-edge-ranciliopid
 * 
 * Based on code of ZACwire-Library written by lebuni/adrian
 * https://github.com/lebuni/ZACwire-Library
 *****************************************************/

#ifndef _gpiocheck_H
#define _gpiocheck_H

#include "Arduino.h"

extern int convertPortModeToDefine(char* portMode);

class GpioCheck {
	public:
		GpioCheck(uint8_t inputPin, char* portMode, unsigned int debounceTime = 30);
		bool begin();					//start reading
		byte getCounter();
		unsigned int getGpioTime();
		void end();
		
	private:
		static void IRAM_ATTR isrHandler(void* ptr);
		void IRAM_ATTR read();
		uint8_t _pin;
		unsigned int _debounceTime;
		char* _portMode;
		volatile unsigned char counter;
		volatile unsigned long gpioTime;  //TODO volatile too big
};

#endif