/*
 * DigitalIOPin.cpp
 *
 *  Created on: Nov 23, 2016
 *      Author: Nick
 */

#include "DigitalIOPin.h"
#include "chip.h"



	// TODO Auto-generated constructor stub
DigitalIoPin :: DigitalIoPin(int setPin, int setPort, bool input, bool pullup, bool invert) {
	port = setPort;
	pin = setPin;

	Chip_IOCON_PinMuxSet(LPC_IOCON, port, pin, (IOCON_MODE_PULLUP | IOCON_DIGMODE_EN | IOCON_INV_EN));
	if (input) {
		Chip_GPIO_SetPinDIRInput(LPC_GPIO, port, pin);
	} else {
		Chip_GPIO_SetPinDIROutput(LPC_GPIO, port, pin);
	}
}
DigitalIoPin :: DigitalIoPin(const DigitalIoPin &io) {
	pin = io.pin;
	port = io.port;
}

DigitalIoPin :: DigitalIoPin(void) {
	pin = 0;
	port = 0;
}


void DigitalIoPin :: write(bool mode){
	Chip_GPIO_SetPinState(LPC_GPIO, port, pin, mode);
}

bool DigitalIoPin :: read(){
	butValue = Chip_GPIO_GetPinState(LPC_GPIO, port, pin);
	return butValue ;
}




