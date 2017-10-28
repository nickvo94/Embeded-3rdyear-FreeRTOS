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

#include <mutex>
#include "Fmutex.h"
#include "user_vcom.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// TODO: insert other definitions and declarations here

SemaphoreHandle_t keepAsking;
SemaphoreHandle_t countingSemaphore;
//SemaphoreHandle_t backQuestion;


/* the following is required if runtime statistics are to be collected */
extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
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

}

char *oracle[6] = {"You will meet a tall dark stranger\r\n",
					"Why would you go there ?\r\n",
					"No need to worry\r\n",
					"It will happen once or twice\r\n",
					"You should choose the 3rd one\r\n",
					"On Thursday, you will see\r\n" };

/* send data and toggle thread */
static void send_task(void *pvParameters) {
	/*bool LedState = false;
	//uint32_t count = 0;
	  char str[15] = "Counter runs!";

	while (1) {
		//int len = sprintf(str, "Counter: %lu runs really fast\r\n", count);
		USB_send((uint8_t *)str, sizeof(str));

		Board_LED_Set(0, LedState);
		LedState = (bool) !LedState;
		//count++;

		vTaskDelay(configTICK_RATE_HZ / 50);
	}*/
	//////////////////////////////////////ex3//////////////////////////////////////////////////////////

	int ansNum;
	//const char temp;
	char str3[20] = "\n\r[Oracle]: " ;
	char str4[60] = "I find your lack of faith disturbing \n\r" ;
	char debug[64];


		while (1) {
			if(xSemaphoreTake(countingSemaphore, portMAX_DELAY) == pdPASS){
				//questions--;
				ansNum = rand()%6;
				USB_send((uint8_t *)str3, sizeof(str3));

				USB_send((uint8_t *)str4, sizeof(str4));

				//newInput = true;
				//xSemaphoreGive(keepAsking);
				vTaskDelay(3000);
				USB_send((uint8_t *)str3, sizeof(str3));


				USB_send((uint8_t *)oracle[ansNum], (uint32_t )strlen(oracle[ansNum]));

				vTaskDelay(2000);

				USB_send((uint8_t*)"\r\n[You]:", 10);
			}

		}

}


/* LED1 toggle thread */
static void receive_task(void *pvParameters) {
	/*bool LedState = false;

	while (1) {
		char str[80];
		uint32_t len = USB_receive((uint8_t *)str, 79);
		str[len] = 0; /* make sure we have a zero at the end so that we can print the data */
	/*	ITM_write(str);
		Board_LED_Set(1, LedState);
		LedState = (bool) !LedState;

	}*/
	//////////////////////////////////////ex3//////////////////////////////////////////////////////////

	//char inChar;
	bool detectq = false;
	char str1[12] = "\r\n[You]: ";
	char str2[4]="\r\n";
	char strRe[RCV_BUFSIZE];

	//USB_send((uint8_t *)str1, sizeof(str1));
	ITM_write("\r\n[You]: ");
	USB_send((uint8_t*)"\r\n[You]: ", 10);

	while (1) {
		uint32_t len = USB_receive((uint8_t *)strRe, RCV_BUFSIZE);
		strRe[len] = 0;
		ITM_write(strRe);
		USB_send((uint8_t*)strRe, len);
		for(uint32_t i=0; i < len; i++){
			if(strRe[i] == '?'){
				detectq = true;
			}
			if(len == 60 || strRe[i] == '\r' || strRe[i] == '\n'  ){
				//ITM_write("\r\n[You]: ");
				USB_send((uint8_t*)str2, 4);
				USB_send((uint8_t*)str1, 12);

				if(detectq == true){
					//questions++;
					xSemaphoreGive(countingSemaphore);
					detectq = false;
				}
			}

		}

	}

}

/*static void vAskingTask(void *pvParameters) {
	char str[10] = "[You]: ";
	while (1) {
		if(xSemaphoreTake(keepAsking, portMAX_DELAY) == pdPASS){
			USB_send((uint8_t *)str, sizeof(str));
			}
		}
}*/



int main(void) {

	prvSetupHardware();
	ITM_init();

	countingSemaphore = xSemaphoreCreateCounting(10, 0);
	keepAsking = xSemaphoreCreateBinary();
	//backQuestion = xSemaphoreCreateBinary();

	/*uint32_t tickrate;
	tickrate = configCPU_CLOCK_HZ;
	char debugOut[80];
	sprintf(debugOut, "tickrate is: %d \n", tickrate);
	ITM_write(debugOut);*/

	/* LED1 toggle thread */
	xTaskCreate(send_task, "Tx",
				configMINIMAL_STACK_SIZE *2, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* LED1 toggle thread */
	xTaskCreate(receive_task, "Rx",
				configMINIMAL_STACK_SIZE *2, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* LED2 toggle thread */
	xTaskCreate(cdc_task, "CDC",
				configMINIMAL_STACK_SIZE *2, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/*xTaskCreate(vAskingTask, "vTaskAsking",
				configMINIMAL_STACK_SIZE *2 , NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);*/


	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}
