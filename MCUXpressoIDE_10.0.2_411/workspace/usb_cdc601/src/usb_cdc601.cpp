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

#include <cr_section_macros.h>

// TODO: insert other include files here
#include "FreeRTOS.h"
#include "task.h"
#include "ITM_write.h"
#include "DigitalIOPin.h"
#include <queue.h>

#include <mutex>
#include "Fmutex.h"
#include "user_vcom.h"
#include <sstream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
// TODO: insert other definitions and declarations here

volatile uint32_t RIT_count;
xSemaphoreHandle sbRIT;

SemaphoreHandle_t left;
SemaphoreHandle_t right;
SemaphoreHandle_t move;
SemaphoreHandle_t hitSW;

//QueueSetHandle_t queueLine;




/* the following is required if runtime statistics are to be collected */
extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(256) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
	}
}
/* end runtime statictics collection */

/* Sets up system hardware */
static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();
	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
	// initialize RIT (= enable clocking etc.)
	Chip_RIT_Init(LPC_RITIMER);
	// set the priority level of the interrupt
	// The level must be equal or lower than the maximum priority specified in FreeRTOS config
	// Note that in a Cortex-M3 a higher number indicates lower interrupt priority
	NVIC_SetPriority( RITIMER_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1 );
}

static void moveTask(void *pvParameters){
	DigitalIoPin portDir (0, 1, false, true, false);
	DigitalIoPin portStep (24, 0, false, true, false);

	while(1){
//--------------------------------------------------
		if(xSemaphoreTake(left,1) == pdPASS){portDir.write(true);}
			if(xSemaphoreTake(right,1) == pdPASS){portDir.write(false);}
			if(xSemaphoreTake(hitSW,1)){portStep.write(false);RIT_count = 0;}
			if(xSemaphoreTake(move,1)== pdPASS){
				//ITM_write("Enter Move task \r\n");
			}else{portStep.write(false);}
			vTaskDelay(1);
//---------------------------------------------------
	}

}

static void vReadSWTask1(void *pvParameters) {
	DigitalIoPin sw1 (27, 0, true, true, false);
	DigitalIoPin sw2 (28, 0, true, true, false);

	//int pressVal = 0; // when release
	bool sw1press;
	bool sw2press;
	bool sw1release;
	bool sw2release;

		while (1) {
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

extern "C" {
void RIT_IRQHandler(void){
	DigitalIoPin portStep (24, 0, false, true, false);
	// This used to check if a context switch is required
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	// Tell timer that we have processed the interrupt.
	// Timer then removes the IRQ until next match occurs
	Chip_RIT_ClearIntStatus(LPC_RITIMER); // clear IRQ flag
	if(RIT_count > 0) {
		RIT_count--;
		// do something useful here...
		//xSemaphoreGive(move);
		portStep.write(true);
		portStep.write(false);

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

	ITM_write("Timer start \r\n");

	// Determine approximate compare value based on clock rate and passed interval
	cmp_value = (uint64_t) Chip_Clock_GetSystemClockRate() * (uint64_t) us / 1000000;
	sprintf(debugStr, "cmp_value = %d \r\n", (uint32_t)cmp_value);
	ITM_write(debugStr);
	// disable timer during configuration
	Chip_RIT_Disable(LPC_RITIMER);
	RIT_count = count;
	// enable automatic clear on when compare value==timer value
	// this makes interrupts trigger periodically
	Chip_RIT_EnableCompClear(LPC_RITIMER);
	// reset the counter
	ITM_write("Timer SetCounter timer \r\n");
	Chip_RIT_SetCounter(LPC_RITIMER, 0);
	Chip_RIT_SetCompareValue(LPC_RITIMER, cmp_value);
	// start counting
	ITM_write("Timer Enable \r\n");
	Chip_RIT_Enable(LPC_RITIMER);
	// Enable the interrupt signal in NVIC (the interrupt controller)
	ITM_write("Timer EnableIRQ \r\n");
	NVIC_EnableIRQ(RITIMER_IRQn);
	// wait for ISR to tell that we're done
	ITM_write("Wait for ISR SemaphoreTake \r\n");

	if(xSemaphoreTake(sbRIT, portMAX_DELAY) == pdTRUE) {
		// Disable the interrupt signal in NVIC (the interrupt controller)
		ITM_write("Received ISR SemaphoreTake, disable timer. \r\n");
		NVIC_DisableIRQ(RITIMER_IRQn);
		ITM_write("Disabled.\r\n");
	}
	else {
		// unexpected error
		ITM_write("Error happens \r\n");
	}

	ITM_write("Exit RIT start \r\n");
}

/* LED1 toggle thread */
static void receive_task(void *pvParameters) {
	//char inChar;
	//char str2[5] = "\r\n";
	char strRe[60] ;
	char keyLeft[] = "left";
	char keyRight[] = "right";
	char keyPulse[] = "pps";

	char typeIn[60] = "\n";
	char leftRep[20] = "\r\nleft received\r\n";
	char rightRep[20] = "\r\nright received\r\n";
	char PulseRep[20] = "\r\npulse change \r\n";


	char *slice;
	char *number;
	char *pulse ;
	int  strPos = 0;
	int pps = 400;

	USB_send((uint8_t *)"\r\n Start : ", 15);
	//RIT_start(100, 10);

	while (1) {
		uint32_t len = USB_receive((uint8_t *)strRe, 59);
		strRe[len] = 0;

		if (len) {
			ITM_write(strRe);
			USB_send((uint8_t*)strRe, len);
			if(strRe[0] == 13){
				USB_send((uint8_t *)"\r\n", 2);
				typeIn[strPos] = 0;
				typeIn[strPos+1] = 10;
				typeIn[strPos+2] = 13;
				ITM_write(typeIn);
				USB_send((uint8_t *)typeIn, strPos + 3);
				strPos = 0;
				slice = strtok(typeIn," \r\n");

				if( strcmp (slice, keyPulse) == 0){
					pulse = strtok(NULL," \r\n");
					pps = atoi(pulse);
					USB_send((uint8_t *)PulseRep, sizeof(PulseRep));
				}


				if( strcmp (slice, keyLeft) == 0){
					number = strtok(NULL," \r\n");
					xSemaphoreGive(left);
					//xSemaphoreGive(move);
					USB_send((uint8_t *)leftRep, sizeof(leftRep));
					RIT_start(atoi(number), (1000000/(2*pps))); // start from 400
				}

				if( strcmp (slice, keyRight) == 0){
					number = strtok(NULL," \r\n");
					xSemaphoreGive(right);
					//xSemaphoreGive(move);
					USB_send((uint8_t *)rightRep, sizeof(rightRep));
					RIT_start(atoi(number),(1000000/(2*pps)));
				}


			} else {
				typeIn[strPos] = strRe[0];
				strPos++;
				if (strPos > 50) strPos = 50;
			}
		}
	}
	vTaskDelay(1);

}




int main(void) {

	prvSetupHardware();
	ITM_init();

	left = xSemaphoreCreateBinary();
	right = xSemaphoreCreateBinary();
	move = xSemaphoreCreateBinary();
	hitSW = xSemaphoreCreateBinary();

	//queueLine = xQueueCreate(20, sizeof(int));

	sbRIT = xSemaphoreCreateBinary();

	xTaskCreate(vReadSWTask1, "vTask1ReadSW",
						configMINIMAL_STACK_SIZE * 2 , NULL, (tskIDLE_PRIORITY + 1UL),
						(TaskHandle_t *) NULL);


	xTaskCreate(moveTask, "MT",
					configMINIMAL_STACK_SIZE * 2, NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);


	xTaskCreate(receive_task, "Rx",
				configMINIMAL_STACK_SIZE * 4, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);


	xTaskCreate(cdc_task, "CDC",
				configMINIMAL_STACK_SIZE * 2, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();
	/* Should never arrive here */
	return 1;
}
