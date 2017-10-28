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
#include <stdint.h>


QueueSetHandle_t syslog_q;



struct debugEvent {
	char *format;
	uint32_t data[3];
};

char *debugFormat[2] = {
		"Received %d characters at %d ms\n",
		"Button is pressed %d ms\n"
};


void debug(char *format, uint32_t d1, uint32_t d2, uint32_t d3){
	debugEvent e;
	e.format = format;
	e.data[0] = d1;
	e.data[1] = d2;
	e.data[2] = d3;
	xQueueSendToBack(syslog_q, &e, portMAX_DELAY);
}

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

Syslog logger = Syslog();

//one priority
static void vEchoTask(void *pvParameters) {
	char inChar;
		uint32_t countChar = 0;
		debugEvent e;
		//e.format = "Received echo task: %d at %d\n" ;
		//char *format = "Received echo task: %d at %d\n" ;
		while (1) {
		inChar = Board_UARTGetChar();
		//print Ascii chart
		if ((inChar > 31) && (inChar < 128) )
		{
			countChar++;
			logger.writeChar(inChar);

			//When enter, no count
			if(inChar == 32){
				countChar = countChar - 1;
				//send number of counted characters
				//e.data[0] = countChar;
				//e.data[1] = xTaskGetTickCount();
				ITM_write("Keyboard event sent.\n");

				//xQueueSendToBack(syslog_q, &e, portMAX_DELAY);
				debug( debugFormat[0], countChar, xTaskGetTickCount(), 1);
				countChar = 0;

			}
		}
		vTaskDelay(10);
	}
}

//other priority
static void vSWTask1(void *pvParameters) {
	DigitalIoPin button1 (17, 0, true, true, false);
	int pressVal = 0; // when release
	debugEvent e;
	//e.format = "Received sw task: %d ms\n " ;
	//char format = "Received sw task: %d ms\n " ;
	uint32_t tickCount = 0;
	uint32_t deltaTime = 0;
	uint32_t start = 0;
	bool button1press;

		while (1) {
			//read button 1
			button1press = button1.read();
			if (button1press == true && pressVal == 0 ){
				start = xTaskGetTickCount();;
			}

			if (button1press == true && pressVal == 1 ){
				tickCount = xTaskGetTickCount();;
				deltaTime = tickCount - start;
			}

			if (button1press == false && pressVal == 1){
				//e.data[0] = deltaTime;
				//e.data[2] = 0;
				ITM_write("Switch event sent.\n");
				//xQueueSendToBack(syslog_q, &e, portMAX_DELAY);
				debug( debugFormat[1], deltaTime, 0, 2);
			}

			if (button1press == false){
				pressVal = 0;
			}else{pressVal = 1;}

			vTaskDelay(10);

		}
}

//Debug print task


void debugTask(void *pvParameters)
{
	char buffer[64];
	debugEvent e;
	while (1) {
		// read queue
		//xQueueReceive(syslog_q, &e, 0);
		xQueueReceive(syslog_q, &e, portMAX_DELAY);
		if(e.data[2]== 1){
			snprintf(buffer, 64, e.format, e.data[0], e.data[1]);
			ITM_write(buffer);
		}
		if(e.data[2]== 2){
			snprintf(buffer, 64, e.format, e.data[0]);
			ITM_write(buffer);
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
	//ITM_write("ITM started \r\n");
	syslog_q = xQueueCreate(5, sizeof(debugEvent));

	//DEBUGOUT("Start \r\n");
	logger.write("Started: \r\n");

	xTaskCreate(vEchoTask, "vTaskEcho",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vSWTask1, "vTaskSW1",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(debugTask, "vTaskdebug",
					configMINIMAL_STACK_SIZE +512, NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	vTaskStartScheduler();


	return 1;
}

