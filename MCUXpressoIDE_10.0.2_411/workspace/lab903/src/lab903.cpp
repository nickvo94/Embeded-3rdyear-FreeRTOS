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
				release = false;
				xEventGroupSetBits(eventSW, (1<<0));
				ITM_write("SW1 pressed.\n");
			}

		}else{release = true;}

	}

}

static void vTask2(void *pvParameters) {
	DigitalIoPin button2 (11, 1, true, true, false);
	bool release = true;
	vTaskDelay(1000);

	while (1) {
		if(button2.read())
		{
			if(release)
			{
				release = false;
				xEventGroupSetBits(eventSW, (1<<1));
				ITM_write("SW2 pressed.\n");
			}

		}else{release = true;}
		vTaskDelay(1);
	}
}

//Debug print task
static void vTask3(void *pvParameters) {
	DigitalIoPin button3 (9, 1, true, true, false);
	bool release = true;

	while (1) {
		if(button3.read())
		{
			if(release)
			{
				release = false;
				xEventGroupSetBits(eventSW, (1<<2));
				ITM_write("SW3 pressed.\n");
			}

		}else{release = true;}
		vTaskDelay(1);
	}
}

static void vTask4(void *pvParameters) {

	EventBits_t eBit;
	EventBits_t waitBit = 7;

	char debug[30];
	int last_succed;
	int start_time;
	int fail_time;

	while (1) {
		start_time = xTaskGetTickCount();
		eBit = xEventGroupWaitBits(eventSW, waitBit, pdTRUE, pdTRUE, 10000);

		if(( eBit & ( 1<<0 | 1<<1 | 1<<2 ) ) == ( 1<<0 | 1<<1 | 1<<2 ))
		{
			int elapsed = xTaskGetTickCount() - last_succed;
			sprintf(debug,"OK %d ms.\r\n", elapsed);
			logger.write(debug);
			last_succed= xTaskGetTickCount();
		}else
		{
			sprintf(debug,"Fail.\r\n");
			logger.write(debug);
		}


		if ( (eBit & (1<<0)) != (1<<0) ) {
			fail_time = xTaskGetTickCount() - start_time;
			sprintf(debug,"Task 1 fail at %d ms.\r\n", fail_time);
			logger.write(debug);
		}
		if ( (eBit & (1<<1)) != (1<<1) ) {
			fail_time = xTaskGetTickCount() - start_time;
			sprintf(debug,"Task 2 fail at %d ms.\r\n", fail_time);
			logger.write(debug);
		}

		if ( (eBit & (1<<2)) != (1<<2) ) {
			fail_time = xTaskGetTickCount() - start_time;
			sprintf(debug,"Task 3 fail %d ms.\r\n", fail_time);
			logger.write(debug);
		}

		xEventGroupClearBits(eventSW, 7);
		vTaskDelay(1);
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

