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
//#include "FreeRTOSConfig.h"
#include "task.h"
#include <stdlib.h>


SemaphoreHandle_t hitSW;
SemaphoreHandle_t go;
SemaphoreHandle_t hitBothSW ;



QueueSetHandle_t queueLine;

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

Syslog logger = Syslog();

static void vRandTask(void *pvParameters) {
	uint8_t data = 2;
	uint32_t len = 5;

	while (1) {



	}
}

static void vSWTask1(void *pvParameters) {
	DigitalIoPin button1 (17, 0, true, true, false);
	int buttonValue = 112;
	int pressVal = 0;
	bool button1press;
		while (1) {
			//read button 1
			button1press = button1.read();
			if (button1press == true && pressVal == 0 ){
				xQueueSendToBack(queueLine, &buttonValue, portMAX_DELAY);

			}
			if (button1press == false){
				pressVal = 0;
			}else{pressVal = 1;}
		}
}

static void vPrintTask(void *pvParameters) {
	int getNum ;
	while (1) {
		xQueueReceive(queueLine, &getNum, portMAX_DELAY );
		DEBUGOUT("\r\n %d \r\n", getNum);
		if(getNum == 112){
			DEBUGOUT("\r\n Help me ! \r\n");
			vTaskDelay(300);
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
	queueLine = xQueueCreate(20, sizeof(int));
	portGET_RUN_TIME_COUNTER_VALUE();


	//DEBUGOUT("Start \r\n");
	logger.write("Started: \r\n");

	xTaskCreate(vRandTask, "vTaskRand",
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
