/*
 * syslog.cpp
 *
 *  Created on: Aug 25, 2017
 *      Author: Nick
 */

#include "syslog.h"
#include <mutex>
#include "FreeRTOS.h"
#include <semphr.h>


Syslog::Syslog() {
	syslogMutex = xSemaphoreCreateMutex();
}
Syslog::~Syslog() {
	vSemaphoreDelete(syslogMutex);
}
void Syslog::write(const char *description) {
	if(xSemaphoreTake(syslogMutex, portMAX_DELAY) == pdTRUE) {
		Board_UARTPutSTR(description);
		xSemaphoreGive(syslogMutex);
	}
}
void Syslog::writeChar(char description) {
	if(xSemaphoreTake(syslogMutex, portMAX_DELAY) == pdTRUE) {
		Board_UARTPutChar(description);
		xSemaphoreGive(syslogMutex);
	}
}

