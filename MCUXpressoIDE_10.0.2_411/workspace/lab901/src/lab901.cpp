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
	bool release = true;
	while(1)
	{
		if(button1.read())
		{
			if(release)
			{
				xEventGroupSetBits(eventSW, (1<<0));
				release = false;
			}

		}else{release = true;}
	}

}

static void vTask2(void *pvParameters) {

	EventBits_t eBit;
	EventBits_t waitBit = (1<<0);

	char debug[30];

	while (1) {
		eBit = xEventGroupWaitBits(eventSW, waitBit, pdTRUE, pdTRUE, portMAX_DELAY);

		int start_ticks = xTaskGetTickCount();
		int interval = rand()% 1000 + 1000;
		vTaskDelay(interval);
		if((eBit & waitBit) != 0)
		{
			int elapsed = xTaskGetTickCount() - start_ticks;
			sprintf(debug,"Task 2 wait %d ms.\r\n", elapsed);
			logger.write(debug);

		}
		vTaskDelay(5);
	}
}

//Debug print task
static void vTask3(void *pvParameters) {

	EventBits_t eBit;
	EventBits_t waitBit = (1<<0);

	char debug[30];

	while (1) {
		eBit = xEventGroupWaitBits(eventSW, waitBit, pdTRUE, pdTRUE, portMAX_DELAY);

		int start_ticks = xTaskGetTickCount();
		int interval = rand()% 1000 + 1000;
		vTaskDelay(interval);
		if((eBit & waitBit) != 0)
		{
			int elapsed = xTaskGetTickCount() - start_ticks;
			sprintf(debug,"Task 3 wait %d ms.\r\n", elapsed);
			logger.write(debug);
		}
		vTaskDelay(5);
	}
}

static void vTask4(void *pvParameters) {

	EventBits_t eBit;
	EventBits_t waitBit = (1<<0);

	char debug[30];

	while (1) {
		eBit = xEventGroupWaitBits(eventSW, waitBit, pdTRUE, pdTRUE, portMAX_DELAY);

		int start_ticks = xTaskGetTickCount();
		int interval = rand()% 1000 + 1000;
		vTaskDelay(interval);
		if((eBit & waitBit) != 0)
		{
			int elapsed = xTaskGetTickCount() - start_ticks;
			sprintf(debug,"Task 4 wait %d ms.\r\n", elapsed);
			logger.write(debug);

		}
		vTaskDelay(5);
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
	srand(time(NULL));

	xTaskCreate(vTask1, "vTask1",
				configMINIMAL_STACK_SIZE *2 , NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	xTaskCreate(vTask2, "vTask2",
					configMINIMAL_STACK_SIZE *3 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vTask3, "vTask3",
					configMINIMAL_STACK_SIZE *3 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vTask4, "vTask4",
					configMINIMAL_STACK_SIZE *3 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);


	vTaskStartScheduler();


	return 1;
}

