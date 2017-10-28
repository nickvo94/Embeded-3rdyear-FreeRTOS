/*
===============================================================================

 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include "DigitalIOPin.h"
#include <cr_section_macros.h>
#include <mutex>
#include "syslog.h"
#include <semphr.h>
#include <queue.h>
#include "ITM_write.h"
#include <time.h>
#include "user_vcom.h"
#include <stdio.h>

//#include <iostream>
#include <string.h>

// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>
#include "event_groups.h"
#include "timers.h"


QueueHandle_t qMonitor;
QueueHandle_t qLed;


#define mainONE_SHOT_TIMER_PERIOD 		( 20000 )
#define mainAUTO_RELOAD_TIMER_PERIOD 	( 5000 )

TimerHandle_t AutoReloadTimer;
TimerHandle_t OneShotTimer;

bool state = false;

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();
	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

Syslog logger = Syslog();


//print task
static void receive_task(void *pvParameters) {
	 xTimerStart(AutoReloadTimer, portMAX_DELAY);
	 xTimerStart(OneShotTimer, portMAX_DELAY);

	char strRe[RCV_BUFSIZE];
	char typeIn[60] = "\n";
	const char helpKey[] = "help";
	const char timeKey[] = "time";
	const char intervalKey[] = "interval";
	char str[] = "\t interval <number> - set toogle interval in seconds \r\n\t "
	             "time - print the number of seconds since the last toggle\r\n";

	char *slice;
	char *interval_char;
	int monitor = 1;
	int  strPos = 0;
	uint32_t last_len;

	while(1)
	{

		uint32_t len = USB_receive((uint8_t *)strRe, RCV_BUFSIZE);
		strRe[len] = 0;
		//ITM_write(strRe);
		USB_send((uint8_t*)strRe, len);

		xQueueReceive(qMonitor,&monitor,0);
		if (monitor != 1) {
			// No char received in 30 seconds
			strRe[0] = '\0'; // reset buffer
			strPos = 0;
			// Reset timer
			monitor = 1;
			xTimerReset(OneShotTimer, portMAX_DELAY);
		}

		if (len > 0)
		{
			//reset one shot if there is input
			xTimerReset(OneShotTimer, portMAX_DELAY);
			last_len = len;
			if((strRe[0] == 13)){
				USB_send((uint8_t *)"\r\n", 2);
				typeIn[strPos] = 0;
				typeIn[strPos+1] = 10;
				typeIn[strPos+2] = 13;
				ITM_write(typeIn);
				USB_send((uint8_t *)typeIn, strPos + 3);
				strPos = 0;

				slice = strtok(typeIn, " \r\n");
				if (strcmp (slice, helpKey) == 0)
				{
					USB_send((uint8_t*)str, sizeof(str));
				}

				if (strcmp (slice, intervalKey) == 0)
				{
					interval_char = strtok(NULL," \r\n");
					// Update toggle interval
					int interval = atoi(interval_char);

					xTimerChangePeriod(AutoReloadTimer, interval, portMAX_DELAY);
					xTimerReset(AutoReloadTimer, portMAX_DELAY);

				}

				if (strcmp (slice, timeKey) == 0)
				{
					TickType_t getTime = 0;
					xQueueReceive(qLed,&getTime,portMAX_DELAY);

					TickType_t now = xTaskGetTickCount();
					float seconds = (now - getTime) / 1000.0;
					char buffer[8] = "       ";
					snprintf(buffer, 8, "%.1f\r\n", seconds);
					USB_send((uint8_t*)buffer, 8);
				}
			}
			else
			{
				typeIn[strPos] = strRe[0];
				strPos++;
				if (strPos > 50) strPos = 50;
			}

		}
		vTaskDelay(1);
	}

}

//receive semaphore and set give command


static void prvOneShotTimerCallback( TimerHandle_t xTimer )
{
	char str[] = "[Inactive]\r\n";
	USB_send((uint8_t*)str, sizeof(str));
	USB_send((uint8_t *)"\r\n", 2);

	int n = 0;
	xQueueOverwrite(qMonitor, &n);

}

static void prvAutoReloadTimerCallback( TimerHandle_t xTimer )
{

	Board_LED_Toggle(1);
	TickType_t now = xTaskGetTickCount();
	xQueueOverwrite(qLed, &now);

}


extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
	}
}

int main(void)
{
	prvSetupHardware();
	ITM_init();

	qLed = xQueueCreate(1, sizeof(TickType_t));
	qMonitor = xQueueCreate(1, sizeof(int));


	OneShotTimer = xTimerCreate("One Shot",	10000,
									pdFALSE, (void*)0,prvOneShotTimerCallback );
	/* Create the auto-reload timer, storing the handle to the created timer in xAutoReloadTimer. */
	AutoReloadTimer = xTimerCreate("Auto Reload", 5000,
									pdTRUE,	(void*)0, prvAutoReloadTimerCallback );

	xTaskCreate(receive_task, "Rx",
					configMINIMAL_STACK_SIZE *6, NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(cdc_task, "CDC",
					configMINIMAL_STACK_SIZE *3, NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);



	vTaskStartScheduler();


	return 1;
}

