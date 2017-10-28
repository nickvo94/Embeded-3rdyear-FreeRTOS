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
// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>


SemaphoreHandle_t hitSW;

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

Syslog logger = Syslog();


static void vReadSWTask1(void *pvParameters) {
	DigitalIoPin sw1 (27, 0, true, true, false);
	DigitalIoPin sw2 (28, 0, true, true, false);

	//int pressVal = 0; // when release
	bool sw1press;
	bool sw2press;

		while (1) {

			/*if (sw1press == true || sw2press == true ){
				xSemaphoreGive(hitSW);
			}*/

			//read sw1
			sw1press = sw1.read();
			if (sw1press == true ){
				Board_LED_Set(0, true);
				xSemaphoreGive(hitSW);

			}
			if (sw1press == false){
				Board_LED_Set(0, false);
			}
			//vTaskDelay(1);

			//read sw2
			sw2press = sw2.read();
			if (sw2press == true){
				Board_LED_Set(1, true);
				xSemaphoreGive(hitSW);
			}
			if (sw2press == false){
				Board_LED_Set(1, false);
			}

			vTaskDelay(1);

		}
}

//Debug print task
static void vMoveTask2(void *pvParameters) {
	DigitalIoPin button1 (17, 0, true, true, false);
	DigitalIoPin button3 (9, 1, true, true, false);
	DigitalIoPin portStep (24, 0, false, true, false);
	DigitalIoPin portDir (0, 1, false, true, false);

	//int pressVal = 0; // when release
	bool button1press;
	bool button3press;

		while (1) {

			//read sw1
			button1press = button1.read();
			if (button1press == true ){
				if(xSemaphoreTake(hitSW, 1) == pdPASS){
					portStep.write(false);
					vTaskDelay(1);
				}else{
					portStep.write(true);
					vTaskDelay(1);
					portDir.write(false);
				}

				//vTaskDelay(1);
			}
			if (button1press == false){
				portStep.write(false);
			}
			//vTaskDelay(5);

			//read sw2
			button3press = button3.read();
			if (button3press == true){
				if(xSemaphoreTake(hitSW, 1) == pdPASS){
					portStep.write(false);
					vTaskDelay(1);
				}else{
					portStep.write(true);
					vTaskDelay(1);
					portDir.write(true);
				}

				//vTaskDelay(1);
			}
			if (button3press == false){
				portStep.write(false);
			}

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
	hitSW = xSemaphoreCreateBinary();
	ITM_init();
	//ITM_write("ITM started \r\n");

	//DEBUGOUT("Start \r\n");
	logger.write("Started: \r\n");



	xTaskCreate(vReadSWTask1, "vTask1ReadSW",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vMoveTask2, "vMoveTask2",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);


	vTaskStartScheduler();


	return 1;
}

