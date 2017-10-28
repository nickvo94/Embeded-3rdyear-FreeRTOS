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

xSemaphoreHandle write;

// TODO: insert other definitions and declarations here


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

/* send data and toggle thread */
static void send_task(void *pvParameters) {
	bool LedState = false;
	while (1) {
		char str[32];
		if(xSemaphoreTake(write,1) == pdPASS)USB_send((uint8_t*)"OK\n", 3);

		//int len = sprintf(str, "Counter: %lu runs really fast\r\n", count);

		vTaskDelay(configTICK_RATE_HZ / 50);
	}
}


/* LED1 toggle thread */
static void receive_task(void *pvParameters) {
	bool LedState = false;

	while (1) {
		char str[80];
		uint32_t len = USB_receive((uint8_t *)str, 79);
		str[len] = 0; /* make sure we have a zero at the end so that we can print the data */
		//USB_send((uint8_t *)str, len);
		if (len >0){xSemaphoreGive(write);}
		ITM_write(str);

		Board_LED_Set(1, LedState);
		LedState = (bool) !LedState;
	}
}


int main(void) {

	prvSetupHardware();
	ITM_init();
	write = xSemaphoreCreateBinary();

	/* LED1 toggle thread */
	xTaskCreate(send_task, "Tx",
				configMINIMAL_STACK_SIZE * 3, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* LED1 toggle thread */
	xTaskCreate(receive_task, "Rx",
				configMINIMAL_STACK_SIZE * 3, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* LED2 toggle thread */
	xTaskCreate(cdc_task, "CDC",
				configMINIMAL_STACK_SIZE * 3, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);


	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}
