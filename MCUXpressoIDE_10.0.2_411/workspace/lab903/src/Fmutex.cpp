/*
 * Fmutex.cpp
 *
 *  Created on: Aug 23, 2017
 *      Author: Nick
 */

#include "Fmutex.h"
#include <mutex>
#include "FreeRTOS.h"
#include <semphr.h>

Fmutex::Fmutex() {
	mutex = xSemaphoreCreateMutex();
}
Fmutex::~Fmutex() {
/* delete semaphore */
/* (not needed if object lifetime is known
* to be infinite) */
}
void Fmutex::lock() {
	xSemaphoreTake(mutex, portMAX_DELAY);
}
void Fmutex::unlock() {
	xSemaphoreGive(mutex);
}

