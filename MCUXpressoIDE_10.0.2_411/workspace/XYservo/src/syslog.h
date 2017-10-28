/*
 * syslog.h
 *
 *  Created on: Aug 25, 2017
 *      Author: Nick
 */

#ifndef SYSLOG_H_
#define SYSLOG_H_
#include <mutex>
#include "FreeRTOS.h"
#include <semphr.h>

class Syslog {
public:
	Syslog();
	virtual ~Syslog();
	void write(const char *description);
	void writeChar(char description);
private:
	SemaphoreHandle_t syslogMutex;
};



#endif /* SYSLOG_H_ */
