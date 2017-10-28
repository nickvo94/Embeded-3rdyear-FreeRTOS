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

// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Sets up system hardware */
static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

/* LED1 toggle thread */
static void vLEDTask1(void *pvParameters) {
	bool LedState = false;
	int redCnt = 0;

	while (1) {
		Board_LED_Set(0, LedState);
		LedState = (bool) !LedState;
		redCnt++;
		//DEBUGOUT("red: %02d \r\n", redCnt);

		/* About a 3Hz on/off toggle rate */
		if(redCnt <  6 || redCnt > 11){
			vTaskDelay(configTICK_RATE_HZ / 8);
		}
		if(redCnt >= 6 && redCnt <= 11){
			vTaskDelay(configTICK_RATE_HZ / 4); //6
		}
		if(redCnt == 18){redCnt = 0;}
	}
}

/* LED2 toggle thread */
static void vLEDTask2(void *pvParameters) {
	bool       LedState = false;
	TickType_t Length   = (configTICK_RATE_HZ / 8) * (6 + 12 + 6);
	while (1) {
		Board_LED_Set(1, LedState);
		LedState = (bool) !LedState;

		/* About a 7Hz on/off toggle rate */
		vTaskDelay(Length); //14
	}
}

/* UART (or output) thread */
static void vUARTTask(void *pvParameters) {
	int tickCnt = 0;
	int min = 0;
	DigitalIoPin button1 (17, 0, true, true, false);

	while (1) {
		bool button1press;
		button1press = button1.read();

		if (button1press == true ){tickCnt = tickCnt + 10;}else{tickCnt++;}

		if (tickCnt > 59){
			tickCnt = tickCnt - 60;
			min++;
		}

		DEBUGOUT("Time: %02d:%02d \r\n", min, tickCnt);

		/* About a 1s delay here */
		vTaskDelay(configTICK_RATE_HZ);
	}
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* the following is required if runtime statistics are to be collected */
extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}

}
/* end runtime statictics collection */

/**
 * @brief	main routine for FreeRTOS blinky example
 * @return	Nothing, function should not exit
 */
int main(void)
{
	prvSetupHardware();

	/* LED1 toggle thread */
	xTaskCreate(vLEDTask1, "vTaskLed1",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* LED2 toggle thread */
	xTaskCreate(vLEDTask2, "vTaskLed2",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* UART output thread, simply counts seconds */
	xTaskCreate(vUARTTask, "vTaskUart",
				configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

