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
/*
void Syslog::writeString(const std::string description) {
	char *cstr = new char[description.length() + 1];
	strcpy(cstr, description.c_str());
	if(xSemaphoreTake(syslogMutex, portMAX_DELAY) == pdTRUE) {
		Board_UARTPutSTR(cstr);
		xSemaphoreGive(syslogMutex);
	}
	delete [] cstr;
}
*/
void Syslog::writeChar(char description) {
	if(xSemaphoreTake(syslogMutex, portMAX_DELAY) == pdTRUE) {
		Board_UARTPutChar(description);
		xSemaphoreGive(syslogMutex);
	}
}

