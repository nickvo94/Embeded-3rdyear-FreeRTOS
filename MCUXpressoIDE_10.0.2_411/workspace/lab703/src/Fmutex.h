/*
 * Fmutex.h
 *
 *  Created on: Aug 23, 2017
 *      Author: Nick
 */
#ifndef FMUTEX_H_
#define FMUTEX_H_
#include <mutex>
#include "FreeRTOS.h"
#include <semphr.h>

class Fmutex {
public:
	Fmutex();
	virtual ~Fmutex();
	void lock();
	void unlock();
private:
	xSemaphoreHandle mutex;
};

#endif /* FMUTEX_H_ */
