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
SemaphoreHandle_t go;
SemaphoreHandle_t hitBothSW ;

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
	bool sw1release;
	bool sw2release;
	int start = 0;

	while(start == 0){
		ITM_write(" start checking switches. \n ");
		Board_LED_Set(2, true);
		vTaskDelay(100);
		Board_LED_Set(2, false);
		vTaskDelay(100);

		sw1press = sw1.read();
		sw2press = sw2.read();

		if (sw1press == false && sw2press == false ){
			xSemaphoreGive(go);
			start = 1;
			ITM_write(" Done checking switches. \n ");
		}

		vTaskDelay(1);

	}

	while (start == 1) {
		/*if (sw1press == true || sw2press == true ){
			xSemaphoreGive(hitSW);
		}*/

		if (sw1press == true && sw2press == true ){
			xSemaphoreGive(hitBothSW);
		}
		//read sw1
		sw1press = sw1.read();
		if (sw1press == true ){
			Board_LED_Set(0, true);
			if (sw1release) {
				xSemaphoreGive(hitSW);
				vTaskDelay(10);
			}
			sw1release = false;
		}

		if (sw1press == false){
			Board_LED_Set(0, false);
			sw1release = true;
		}

		//read sw2
		sw2press = sw2.read();
		if (sw2press == true){
			Board_LED_Set(1, true);
			if (sw2release) {
				xSemaphoreGive(hitSW);
				vTaskDelay(10);
			}
			sw2release = false;

		}

		if (sw2press == false){
			Board_LED_Set(1, false);
			sw2release = true;
		}

		vTaskDelay(1);

	}
}

//Debug print task
static void vMoveTask2(void *pvParameters) {
	//DigitalIoPin button1 (17, 0, true, true, false);
	//DigitalIoPin button3 (9, 1, true, true, false);
	DigitalIoPin portStep (24, 0, false, true, false);
	DigitalIoPin portDir (0, 1, false, true, false);

	//int pressVal = 0; // when release
	//bool button1press;
	//bool button3press;
	bool direction = true;
	int begin = 0;
	int trialTime = 0;
	int countStep = 0;
	int stepArray [3];
	int averageStep = 0;
	int runningStep = 0;
	char dir_debug[80];

	while(1){
		if(xSemaphoreTake(go, 1) == pdPASS){ITM_write("Receive go.\n ");begin = 1;}

		while (begin == 1) {
			if(xSemaphoreTake(hitBothSW, 1) != pdPASS){
				//ITM_write(" Receive not hitBothSW .\n ");

				portDir.write(direction);

				portStep.write(true);
				vTaskDelay(1);
				portStep.write(false);
				//vTaskDelay(1);

				countStep++;
				//DEBUGOUT("count step : %d .\r\n ", countStep);

				if(xSemaphoreTake(hitSW, 1) == pdPASS){
					//change direction
					sprintf(dir_debug, "Direction: %d , trial time : %d, count step : %d\n", (int)direction, trialTime, countStep);
					ITM_write(" Receive hitSW .\n ");
					ITM_write(dir_debug);

					direction = !direction;
					portDir.write(direction);

					//start to collect step number from 2nd trial
					if(trialTime >0){stepArray[trialTime-1]= countStep;}
					countStep = 0;
					trialTime ++;
					//DEBUGOUT("Trial time : %d .\r\n ", trialTime);

				}
				//ITM_write("Out of loop .\n ");


				// Calculating average step
				if(trialTime == 4){
					trialTime = 0;
					averageStep = (stepArray[0] + stepArray [1] + stepArray [2])/3;
					averageStep = averageStep - 20;
					begin = 2;

					Board_LED_Set(2, true);
					vTaskDelay(2000);
				}
				//vTaskDelay(1);

			}else{ITM_write("Delay.\n");vTaskDelay(5000);}


		}
		Board_LED_Set(2, false);
		vTaskDelay(1);

		//Running without touching SW
		//bool direction2 = true;
		while(begin == 2 ){

			runningStep++;
			portDir.write(direction);

			portStep.write(true);
			vTaskDelay(1);
			portStep.write(false);
			//vTaskDelay(1);

			if(runningStep == averageStep){
				direction = !direction;
				portDir.write(direction);
				runningStep = 20;
			}
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
	go = xSemaphoreCreateBinary();
	hitBothSW = xSemaphoreCreateBinary();
	ITM_init();
	//ITM_write("ITM started \r\n");

	//DEBUGOUT("Start \r\n");
	logger.write("Started: \r\n");



	xTaskCreate(vReadSWTask1, "vTask1ReadSW",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vMoveTask2, "vMoveTask2",
					configMINIMAL_STACK_SIZE +512 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);


	vTaskStartScheduler();


	return 1;
}
