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
// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"

QueueSetHandle_t countNumber;

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

Syslog logger = Syslog();

static void vEchoTask(void *pvParameters) {
	char inChar;
		int countChar = 0;
		while (1) {
		inChar = Board_UARTGetChar();
		//print Ascii chart
		if ((inChar > 7) && (inChar < 128) )
		{
			countChar++;
			logger.writeChar(inChar);

			//When enter, no count
			if((inChar == 10) || (inChar == 13)){
				countChar = countChar - 1;
				logger.write("\r\n");
				xQueueSendToBack(countNumber, &countChar, portMAX_DELAY);
				countChar = 0;
			}

		}
	}

}

static void vSWTask1(void *pvParameters) {
	DigitalIoPin button1 (17, 0, true, true, false);
	int buttonValue = -1;
	int pressVal = 0;
	bool button1press;
		while (1) {
			//read button 1
			button1press = button1.read();
			if (button1press == true && pressVal == 0 ){
				xQueueSendToBack(countNumber, &buttonValue, portMAX_DELAY);

			}
			if (button1press == false){
				pressVal = 0;
			}else{pressVal = 1;}
		}
}

static void vPrintTask(void *pvParameters) {
	int getNum ;
	int finalValue = 0;
	while (1) {
		xQueueReceive(countNumber, &getNum, portMAX_DELAY );
		if(getNum == -1){
			DEBUGOUT("\r\n You have typed %d characters \r\n",finalValue );
			finalValue = 0;
		}
		else{finalValue += getNum;}

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
	countNumber = xQueueCreate(5, sizeof(int));

	//DEBUGOUT("Start \r\n");
	logger.write("Started: \r\n");

	xTaskCreate(vEchoTask, "vTaskEcho",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vSWTask1, "vTaskSW1",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vPrintTask, "vTaskPrint",
					configMINIMAL_STACK_SIZE +128, NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	vTaskStartScheduler();


	return 1;
}

