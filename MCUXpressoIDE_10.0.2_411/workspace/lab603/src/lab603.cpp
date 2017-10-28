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

volatile uint32_t RIT_count;
xSemaphoreHandle sbRIT;

char motorDebug[50];
bool direction = false;
bool stop = false;
bool run = false;

uint32_t countRIT;
int averageStep = 0;
uint32_t motorStep = 0;
int pps = 600;


SemaphoreHandle_t hitSW;
SemaphoreHandle_t go;
SemaphoreHandle_t goCheck;
SemaphoreHandle_t hitBothSW ;
SemaphoreHandle_t move;

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();
	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
	Board_LED_Set(1, false);
	// initialize RIT (= enable clocking etc.)
	Chip_RIT_Init(LPC_RITIMER);
	// set the priority level of the interrupt
	// The level must be equal or lower than the maximum priority specified in FreeRTOS config
	// Note that in a Cortex-M3 a higher number indicates lower interrupt priority
	NVIC_SetPriority( RITIMER_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1 );
}

extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
	}
}

extern "C" {
void RIT_IRQHandler(void){
	static DigitalIoPin portStep (24, 0, false, true, false);
	DigitalIoPin sw1 (27, 0, true, true, false);
	DigitalIoPin sw2 (28, 0, true, true, false);
	static bool state = true;

	// This used to check if a context switch is required
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	// Tell timer that we have processed the interrupt.
	// Timer then removes the IRQ until next match occurs
	Chip_RIT_ClearIntStatus(LPC_RITIMER); // clear IRQ flag

	if(RIT_count > 0) {
		RIT_count--;

		/*if(run == true){
			if(RIT_count < 2){
				if(!sw1.read() || !sw2.read()){stop = true;RIT_count = 0;}
			}
		}*/


		//countRIT++;
		// do something useful here...
		portStep.write(state);
		state = !state;

		//sprintf(motorDebug, "Count RIT : %d\n", (int)RIT_count);
		//ITM_write(motorDebug);


		}
		else {
			//portStep.write(false);
			Chip_RIT_Disable(LPC_RITIMER); // disable timer
			// Give semaphore and set context switch flag if a higher priority task was woken up
			xSemaphoreGiveFromISR(sbRIT, &xHigherPriorityWoken);
	}
	// End the ISR and (possibly) do a context switch
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
	}
}

void RIT_start(int count, int us)
{
	char debugStr[80];
	uint64_t cmp_value;

	//ITM_write("Timer start \r\n");

	// Determine approximate compare value based on clock rate and passed interval
	cmp_value = (uint64_t) Chip_Clock_GetSystemClockRate() * (uint64_t) us / 1000000;
	sprintf(debugStr, "cmp_value = %d \r\n", (uint32_t)cmp_value);
	//ITM_write(debugStr);
	// disable timer during configuration
	Chip_RIT_Disable(LPC_RITIMER);
	RIT_count = count;
	// enable automatic clear on when compare value==timer value
	// this makes interrupts trigger periodically
	Chip_RIT_EnableCompClear(LPC_RITIMER);
	// reset the counter
	//ITM_write("Timer SetCounter timer \r\n");
	Chip_RIT_SetCounter(LPC_RITIMER, 0);
	Chip_RIT_SetCompareValue(LPC_RITIMER, cmp_value);
	// start counting
	//ITM_write("Timer Enable \r\n");
	Chip_RIT_Enable(LPC_RITIMER);
	// Enable the interrupt signal in NVIC (the interrupt controller)
	//ITM_write("Timer EnableIRQ \r\n");
	NVIC_EnableIRQ(RITIMER_IRQn);
	// wait for ISR to tell that we're done
	//ITM_write("Wait for ISR SemaphoreTake \r\n");

	if(xSemaphoreTake(sbRIT, portMAX_DELAY) == pdTRUE) {
		// Disable the interrupt signal in NVIC (the interrupt controller)
		//ITM_write("Received ISR SemaphoreTake, disable timer. \r\n");
		NVIC_DisableIRQ(RITIMER_IRQn);
		//ITM_write("Disabled.\r\n");
	}
	else {
		// unexpected error
		//ITM_write("Error happens \r\n");
	}

	//ITM_write("Exit RIT start \r\n");
}

static void vReadSWTask1(void *pvParameters) {
	DigitalIoPin sw1 (27, 0, true, true, false);
	DigitalIoPin sw2 (28, 0, true, true, false);
	DigitalIoPin portStep (24, 0, false, true, false);
	DigitalIoPin portDir (0, 1, false, true, false);

	//int pressVal = 0; // when release
	bool sw1press;
	bool sw2press;
	bool sw1release;
	bool sw2release;
	int start = 0;
	char ppsDebug[80];

	while(start == 0){
		Chip_RIT_Disable(LPC_RITIMER);
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

		if(xSemaphoreTake(hitSW,1)){
			//Chip_RIT_Disable(LPC_RITIMER);
			sprintf(ppsDebug, "New pps: %d ", (int)pps);
			ITM_write(ppsDebug);
		}


		if (sw1press == true && sw2press == true ){
			xSemaphoreGive(hitBothSW);
		}
		//read sw1
		sw1press = sw1.read();
		if (sw1press == true ){
			Board_LED_Set(0, true);
			if (sw1release) {
				xSemaphoreGive(hitSW);
				vTaskDelay(500);
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
				vTaskDelay(500);
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
	DigitalIoPin portStep (24, 0, false, true, false);
	DigitalIoPin portDir (0, 1, false, true, false);
	DigitalIoPin sw1 (27, 0, true, true, false);
	DigitalIoPin sw2 (28, 0, true, true, false);

	//int pressVal = 0; // when release
	//bool button1press;
	//bool button3press;

	bool minus = true;
	int stepArray [2];
	int begin = 0;
	int trialTime = 0;
	int countStep = 0;
	int rpm;
	int i = 1;

	char dir_debug[90];
	TickType_t start_time_calibration = 0;
	TickType_t end_time_calibration = 0;
	TickType_t start_time_run = 0;
	TickType_t end_time_run = 0;

	while(1){
		if(xSemaphoreTake(go, 1) == pdPASS){ITM_write("Receive go.\n ");begin = 1;}
		//portDir.write(direction);

		while (begin == 1) {
			//sprintf(dir_debug, "Motor step : %d,count RIT : %d\n", motorStep, (int)countRIT);
			//ITM_write(dir_debug);

			if(xSemaphoreTake(hitBothSW, 1) != pdPASS){
				//ITM_write(" Receive not hitBothSW .\n ");

				//portStep.write(stateBool);
				//vTaskDelay(1);
				//stateBool = !stateBool;
				RIT_start(1, (1000000/(2*400)));
				countStep++;

				//DEBUGOUT("count step : %d .\r\n ", countStep);
				if(xSemaphoreTake(hitSW, 1) == pdPASS){
					//change direction
					Chip_RIT_Disable(LPC_RITIMER);

					direction = !direction;
					portDir.write(direction);

					end_time_calibration = xTaskGetTickCount() - start_time_calibration;

					sprintf(dir_debug, "Direction: %d , trial time : %d, count step : %d, elapse: %4.2f \n", (int)direction, trialTime, countStep, (float)end_time_calibration);
					ITM_write(" Receive hitSW .\n ");
					ITM_write(dir_debug);

					start_time_calibration = 0;
					start_time_calibration = xTaskGetTickCount();

					//start to collect step number from 2nd trial
					if(trialTime >0){stepArray[trialTime-1]= countStep;}
					countStep = 0;
					trialTime ++;

					//DEBUGOUT("Trial time : %d .\r\n ", trialTime);

				}
				//ITM_write("Out of loop .\n ");


				// Calculating average step
				if(trialTime == 3){
					trialTime = 0;
					averageStep = (stepArray[0] + stepArray [1])/2;
					//averageStep = averageStep - 1;
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

		//offset
		if(begin == 2 ){
			//direction = !direction;
			portDir.write(direction);
			motorStep = averageStep ;
			RIT_start(1, (1000000/(pps)));
			//motorStep = motorStep - 1;
			begin = 3;
			}
		//acceleration
		while(begin == 3){
			start_time_run = xTaskGetTickCount();
			//vTaskDelay(200);
			RIT_start(5, (1000000/(pps)));
			RIT_start(5, (1000000/(1.2*pps)));
			RIT_start(5, (1000000/(1.4*pps)));
			RIT_start(5, (1000000/(1.6*pps)));
			RIT_start(5, (1000000/(1.8*pps)));
			if( minus == true){
				motorStep = motorStep - 25;
				minus = false;
			}
			begin = 4;
		}
		//Running
		while(begin == 4)
		{
			//portDir.write(direction);
			run = true;
			RIT_start(motorStep, (1000000/(2*pps)));
			vTaskDelay(60);
			if(!sw1.read() && !sw2.read()){stop = true;}
			direction = !direction;
			portDir.write(direction);
			rpm = 60*pps/400;
			end_time_run = xTaskGetTickCount() - start_time_run;
			sprintf(dir_debug, "PPS: %d, motor : %d, rpm: %d, elapse : %4.2f\n", (int)pps, (int)motorStep, (int)rpm, (float)end_time_run);
			ITM_write(dir_debug);
			while(stop == true){
				Chip_RIT_Disable(LPC_RITIMER);
				if(i == 1){
					ITM_write("Test end.\n");
					sprintf(dir_debug, "Max PPS: %d, motor : %d, rpm: %d, elapse : %4.2f\n", (int)pps, (int)motorStep, (int)rpm, (float)end_time_run);
					ITM_write(dir_debug);
					i = 0;
				}
			}
			pps = pps+200;
			begin = 3;

		}

		/*while(begin == 5){
			//vTaskDelay(200);
			RIT_start(5, (1000000/(1.8*pps)));
			RIT_start(5, (1000000/(1.6*pps)));
			RIT_start(5, (1000000/(1.4*pps)));
			RIT_start(5, (1000000/(1.2*pps)));
			RIT_start(5, (1000000/(pps)));
			if( minus == true){
				motorStep = motorStep - 25;
				minus = false;
			}
			begin = 3;
		}*/

	}
}



int main(void)
{
	prvSetupHardware();
	hitSW = xSemaphoreCreateBinary();
	go = xSemaphoreCreateBinary();
	move = xSemaphoreCreateBinary();
	hitBothSW = xSemaphoreCreateBinary();
	goCheck = xSemaphoreCreateBinary();

	sbRIT = xSemaphoreCreateBinary();

	ITM_init();
	ITM_write("ITM started \r\n");

	xTaskCreate(vReadSWTask1, "vTask1ReadSW",
					configMINIMAL_STACK_SIZE *3 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vMoveTask2, "vMoveTask2",
					configMINIMAL_STACK_SIZE *4 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);


	vTaskStartScheduler();


	return 1;
}
