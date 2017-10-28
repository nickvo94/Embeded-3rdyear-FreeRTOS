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

// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>
#include "event_groups.h"
#include "timers.h"

SemaphoreHandle_t  oneShot;
QueueHandle_t queueLine;

TimerHandle_t AutoReloadTimer;
TimerHandle_t OneShotTimer;

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();
	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

Syslog logger = Syslog();


//print task
static void vTask1(void *pvParameters) {
	int value;
	char debug[8] = "       ";
	while(1)
	{
		xQueueReceive(queueLine,&value,portMAX_DELAY);
		switch(value)
		{
		case 1:
			sprintf(debug,"hello\r\n");
			logger.write(debug);
			break;
		case 2:
			sprintf(debug,"aargh\r\n");
			logger.write(debug);
			break;
		}
	}

}

//receive semaphore and set give command
static void vTask2(void *pvParameters) {

	xTimerStart(OneShotTimer, portMAX_DELAY);
	xTimerStart(AutoReloadTimer, portMAX_DELAY);
	int value = 2;

	while (1) {
		if(xSemaphoreTake(oneShot,portMAX_DELAY))xQueueSendToBack(queueLine, &value, portMAX_DELAY);

	}
}

static void prvOneShotTimerCallback( TimerHandle_t xTimer )
{
	xSemaphoreGive(oneShot);
}

static void prvAutoReloadTimerCallback( TimerHandle_t xTimer )
{
	int value = 1;
	xQueueSendToBack(queueLine,&value,portMAX_DELAY);
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

	oneShot = xSemaphoreCreateBinary();
	queueLine = xQueueCreate(10, sizeof(int));

	OneShotTimer = xTimerCreate("One Shot",	20000,
									pdFALSE, (void*)0,prvOneShotTimerCallback );
	/* Create the auto-reload timer, storing the handle to the created timer in xAutoReloadTimer. */
	AutoReloadTimer = xTimerCreate("Auto Reload", 5000,
									pdTRUE,	(void*)0, prvAutoReloadTimerCallback );

	xTaskCreate(vTask1, "vTask1",
				configMINIMAL_STACK_SIZE *4 , NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	xTaskCreate(vTask2, "vTask2",
					configMINIMAL_STACK_SIZE *4 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	vTaskStartScheduler();


	return 1;
}

