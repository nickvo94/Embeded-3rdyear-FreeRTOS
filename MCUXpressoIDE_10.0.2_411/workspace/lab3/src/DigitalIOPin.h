/*
 * DigitalIOPin.h
 *
 *  Created on: Nov 23, 2016
 *      Author: Vo
 */

#ifndef DIGITALIOPIN_H_
#define DIGITALIOPIN_H_

class DigitalIoPin {
	public:
		DigitalIoPin (int, int, bool, bool, bool);
		DigitalIoPin (const DigitalIoPin &io);
		DigitalIoPin ();
		void write(bool state);
		bool read();
		void ButtonSystem();
	private:
		int port;
		int pin;
		bool butValue;

};


#endif /* DIGITALIOPIN_H_ */
