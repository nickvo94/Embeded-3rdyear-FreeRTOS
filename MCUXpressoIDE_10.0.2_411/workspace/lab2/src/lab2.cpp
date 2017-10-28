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
#include <atomic>
#include <semphr.h>
// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"

//std::atomic<bool> blink (false);
SemaphoreHandle_t Semaphore1;

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

Syslog logger = Syslog();

static void vSWTask1(void *pvParameters) {
	DigitalIoPin button1 (17, 0, true, true, false);
		while (1) {
			bool button1press;
			button1press = button1.read();
			if (button1press == true ){logger.write("SW1 pressed \r\n");}
			vTaskDelay(200);
		}
}

static void vSWTask2(void *pvParameters) {
	DigitalIoPin button2 (11, 1, true, true, false);
		while (1) {
			bool button2press;
			button2press = button2.read();
			if (button2press == true ){logger.write("SW2 pressed \r\n");}
			vTaskDelay(200);
		}
}

static void vSWTask3(void *pvParameters) {
	DigitalIoPin button3 (9, 1, true, true, false);
		while (1) {
			bool button3press;
			button3press = button3.read();
			if (button3press == true ){logger.write("SW3 pressed \r\n");}
			vTaskDelay(200);
		}
}

static void vEchoTask(void *pvParameters) {
	char inChar;
	while (1) {
		inChar = Board_UARTGetChar();

		if ((inChar > 32) && (inChar < 127) )
		{
			logger.write("Keypress:");
			logger.writeChar(inChar);
			logger.write(" ");
			xSemaphoreGive( Semaphore1 );
			//vTaskDelay(200);
			//blink = true;
		}


	}
}

static void vLEDTask(void *pvParameters) {
	while (1) {
		if (xSemaphoreTake( Semaphore1, portMAX_DELAY ) == pdPASS)
		{
			//blink = false;
			Board_LED_Set(1, true);
			vTaskDelay(100); //14
			Board_LED_Set(1, false);
			vTaskDelay(100);
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
	Semaphore1 = xSemaphoreCreateBinary();

	//DEBUGOUT("Start \r\n");
	logger.write("Started: \r\n");

	xTaskCreate(vSWTask1, "vTaskSW1",
				configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);


	xTaskCreate(vSWTask2, "vTaskSW2",
				configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	xTaskCreate(vSWTask3, "vTaskSW3",
				configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	xTaskCreate(vEchoTask, "vTaskEcho",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vLEDTask, "vTaskLed",
					configMINIMAL_STACK_SIZE +128, NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	vTaskStartScheduler();


	return 1;
}

