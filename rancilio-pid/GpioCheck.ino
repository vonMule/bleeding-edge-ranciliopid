#include "GpioCheck.h"


char* createGpioCheck(uint8_t gpio, char* portMode, unsigned int debounceTime) {
	switch (gpio) {
		case 16:
			return (char*)(GpioCheck<16>*) new GpioCheck<16>(portMode, debounceTime);
			break;
#ifdef ESP32
		case 2:
			return (char*)(GpioCheck<2>*) new GpioCheck<2>(portMode, debounceTime);
			break;
		case 4:
			return (char*)(GpioCheck<4>*) new GpioCheck<4>(portMode, debounceTime);
			break;
		case 12:
			return (char*)(GpioCheck<12>*) new GpioCheck<12>(portMode, debounceTime);
			break;
		case 13:
			return (char*)(GpioCheck<13>*) new GpioCheck<13>(portMode, debounceTime);
			break;
		case 17:
			return (char*)(GpioCheck<17>*) new GpioCheck<17>(portMode, debounceTime);
			break;	
		case 18:
			return (char*)(GpioCheck<18>*) new GpioCheck<18>(portMode, debounceTime);
			break;
		case 23:
			return (char*)(GpioCheck<23>*) new GpioCheck<23>(portMode, debounceTime);
			break;
		case 25:
			return (char*)(GpioCheck<25>*) new GpioCheck<25>(portMode, debounceTime);
			break;	
		case 27:
			return (char*)(GpioCheck<27>*) new GpioCheck<27>(portMode, debounceTime);
			break;
		case 32:
			return (char*)(GpioCheck<32>*) new GpioCheck<32>(portMode, debounceTime);
			break;
		case 33:
			return (char*)(GpioCheck<33>*) new GpioCheck<33>(portMode, debounceTime);
			break;	
		case 34:
			return (char*)(GpioCheck<34>*) new GpioCheck<34>(portMode, debounceTime);
			break;
		case 35:
			return (char*)(GpioCheck<35>*) new GpioCheck<35>(portMode, debounceTime);
			break;
#endif
        default:
            DEBUG_println("createGpioCheck() gpio not defined!");
            return NULL;
	}
}

void beginGpioCheck(uint8_t gpio, char* GpioCheckInstance) {
	if (GpioCheckInstance == NULL) {
		DEBUG_print("counterGpioCheck(%u) gpio not instantiated!\n", gpio);
		return;
	}
	switch (gpio) {
		case 16:
			((GpioCheck<16>*)GpioCheckInstance)->begin();
			break;
#ifdef ESP32
		case 2:
            ((GpioCheck<2>*)GpioCheckInstance)->begin();
			break;
		case 4:
			((GpioCheck<4>*)GpioCheckInstance)->begin();
			break;
		case 12:
			((GpioCheck<12>*)GpioCheckInstance)->begin();
			break;
		case 13:
			((GpioCheck<13>*)GpioCheckInstance)->begin();
			break;
		case 17:
			((GpioCheck<17>*)GpioCheckInstance)->begin();
			break;
		case 18:
            ((GpioCheck<18>*)GpioCheckInstance)->begin();
			break;
		case 23:
			((GpioCheck<23>*)GpioCheckInstance)->begin();
			break;
		case 25:
			((GpioCheck<25>*)GpioCheckInstance)->begin();
			break;
		case 27:
			((GpioCheck<27>*)GpioCheckInstance)->begin();
			break;
		case 32:
            ((GpioCheck<32>*)GpioCheckInstance)->begin();
			break;
		case 33:
			((GpioCheck<33>*)GpioCheckInstance)->begin();
			break;
		case 34:
			((GpioCheck<34>*)GpioCheckInstance)->begin();
			break;
		case 35:
			((GpioCheck<35>*)GpioCheckInstance)->begin();
			break;
#endif
        default:
            DEBUG_println("beginGpioCheck() gpio not defined!");
	}
}

unsigned char counterGpioCheck(uint8_t gpio, char* GpioCheckInstance) {
	if (GpioCheckInstance == NULL) {
		DEBUG_print("counterGpioCheck(%u) gpio not instantiated!\n", gpio);
		return 0;
	}
	switch (gpio) {
		case 16:
			return ((GpioCheck<16>*)GpioCheckInstance)->getCounter();
			break;
#ifdef ESP32
		case 2:
			return ((GpioCheck<2>*)GpioCheckInstance)->getCounter();
			break;
		case 4:
			return ((GpioCheck<4>*)GpioCheckInstance)->getCounter();
			break;
		case 12:
			return ((GpioCheck<12>*)GpioCheckInstance)->getCounter();
			break;
		case 13:
			return ((GpioCheck<13>*)GpioCheckInstance)->getCounter();
			break;
		case 17:
			return ((GpioCheck<17>*)GpioCheckInstance)->getCounter();
			break;
		case 18:
			return ((GpioCheck<18>*)GpioCheckInstance)->getCounter();
			break;
		case 23:
			return ((GpioCheck<23>*)GpioCheckInstance)->getCounter();
			break;
		case 25:
			return ((GpioCheck<25>*)GpioCheckInstance)->getCounter();
			break;
		case 27:
			return ((GpioCheck<27>*)GpioCheckInstance)->getCounter();
			break;
		case 32:
			return ((GpioCheck<32>*)GpioCheckInstance)->getCounter();
			break;
		case 33:
			return ((GpioCheck<33>*)GpioCheckInstance)->getCounter();
			break;
		case 34:
			return ((GpioCheck<34>*)GpioCheckInstance)->getCounter();
			break;
		case 35:
			return ((GpioCheck<35>*)GpioCheckInstance)->getCounter();
			break;
#endif
        default:
            DEBUG_print("counterGpioCheck(%u) gpio not defined!\n", gpio);
            return 0;
	}
}
