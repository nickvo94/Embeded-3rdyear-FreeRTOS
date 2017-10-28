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

EventGroupHandle_t eventSW;

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();
	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

Syslog logger = Syslog();



static void vTask1(void *pvParameters) {
	DigitalIoPin button1 (17, 0, true, true, false);
	int task  = 1;
	int count = 1;
	bool release = true;

	EventBits_t eBit;
	const EventBits_t taskBit  = (1<<0);
	const EventBits_t allTaskBits = (1<<0) + (1<<1) + (1<<2);

	char debug[35];

	while(1)
	{
		if(button1.read())
		{
			if(release)
			{
				release = false;
				count--;
				ITM_write("SW1 pressed.\n");
			}

		}else{release = true;}

		if(count == 0)
		{
			xEventGroupSync(eventSW, taskBit, allTaskBits, portMAX_DELAY);
			sprintf(debug,"Task 1 complete %d ms.\r\n", (int)xTaskGetTickCount());
			logger.write(debug);
			count = task;
			//xEventGroupClearBits(eventSW, allTaskBits);
		}
	}

}

static void vTask2(void *pvParameters) {
	DigitalIoPin button2 (11, 1, true, true, false);
	int task  = 2;
	int count = 2;
	bool release = true;

	EventBits_t eBit;
	const EventBits_t taskBit  = (1<<1);
	const EventBits_t allTaskBits = (1<<0) + (1<<1) + (1<<2);

	char debug[35];

	vTaskDelay(1000);

	while(1)
	{
		if(button2.read())
		{
			if(release)
			{
				release = false;
				count--;
				ITM_write("SW2 pressed.\n");
			}

		}else{release = true;}

		if(count == 0)
		{
			xEventGroupSync(eventSW, taskBit, allTaskBits, portMAX_DELAY);
			sprintf(debug,"Task 2 complete %d ms.\r\n", (int)xTaskGetTickCount());
			logger.write(debug);
			count = task;
			//xEventGroupClearBits(eventSW, allTaskBits);
		}
	}
}

//Debug print task
static void vTask3(void *pvParameters) {
	DigitalIoPin button3 (9, 1, true, true, false);
	int task  = 3;
	int count = 3;
	bool release = true;

	EventBits_t eBit;
	const EventBits_t taskBit  = (1<<2);
	const EventBits_t allTaskBits = (1<<0) + (1<<1) + (1<<2);

	char debug[35];

	int debug_ticks = 0;

	while(1)
	{
		/*if ((int)xTaskGetTickCount() > debug_ticks)
		{
			debug_ticks = (int)xTaskGetTickCount() + 2000;
			ITM_write("Task 3 is alive\n");
		}*/
		if(button3.read())
		{
			if(release)
			{
				release = false;
				count--;
				ITM_write("SW3 pressed.\n");
			}

		}else{release = true;}

		if(count == 0)
		{
			eBit = xEventGroupSync(eventSW, taskBit, allTaskBits, portMAX_DELAY);
			sprintf(debug,"Task 3 complete %d ms.\r\n", (int)xTaskGetTickCount());
			logger.write(debug);
			count = task;
			//xEventGroupClearBits(eventSW, allTaskBits);
		}
	}
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

	eventSW = xEventGroupCreate();

	xTaskCreate(vTask1, "vTask1",
				configMINIMAL_STACK_SIZE *4 , NULL, (tskIDLE_PRIORITY + 2UL),
				(TaskHandle_t *) NULL);

	xTaskCreate(vTask2, "vTask2",
					configMINIMAL_STACK_SIZE *4 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vTask3, "vTask3",
					configMINIMAL_STACK_SIZE *4 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);


	vTaskStartScheduler();


	return 1;
}

