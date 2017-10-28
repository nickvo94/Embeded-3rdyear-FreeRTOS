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
#include <stdio.h>
#include "ITM_write.h"
// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"

QueueSetHandle_t queueLine;

DigitalIoPin sw1(17,0, true, true, false);
DigitalIoPin sw2(11,1, true, true, false);
DigitalIoPin sw3(9, 1, true, true, false);

static void Interrupt_init()
{
	// assign interrupts to pins
	Chip_INMUX_PinIntSel(0, 0, 17); 		//sw1
	Chip_INMUX_PinIntSel(1, 1, 11); 		//sw2
	Chip_INMUX_PinIntSel(2, 1, 9); 			//sw3

	// enable clock for GPIO interrupt
	Chip_PININT_Init(LPC_GPIO_PIN_INT);
	// Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_PININT);

	// Enable IRQ
	NVIC_EnableIRQ(PIN_INT0_IRQn);
	NVIC_EnableIRQ(PIN_INT1_IRQn);
	NVIC_EnableIRQ(PIN_INT2_IRQn);

	// Set Interrupt priorities
	NVIC_SetPriority(PIN_INT0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
	NVIC_SetPriority(PIN_INT1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
	NVIC_SetPriority(PIN_INT2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);

	// interrupt at falling edge, clear initial status
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH0);
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH0);
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);

	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH1);
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH1);
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);

	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH2);
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH2);
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH2);

	NVIC_ClearPendingIRQ(PIN_INT1_IRQn);
	NVIC_ClearPendingIRQ(PIN_INT2_IRQn);
	NVIC_ClearPendingIRQ(PIN_INT3_IRQn);

}

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);

	ITM_init();
	Interrupt_init();

	// initialize RIT (= enable clocking etc.)
	Chip_RIT_Init(LPC_RITIMER);
	// set the priority level of the interrupt
	// The level must be equal or lower than the maximum priority specified in FreeRTOS config
	// Note that in a Cortex-M3 a higher number indicates lower interrupt priority
	NVIC_SetPriority( RITIMER_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1 );
}

extern "C" {
	void PIN_INT0_IRQHandler() {
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
		int val = 1;
		BaseType_t xHigherPriorityWoken = pdFALSE;
		xQueueSendToBackFromISR(queueLine,&val,&xHigherPriorityWoken);
	}

	void PIN_INT1_IRQHandler() {
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);
		int val1 = 2;
		BaseType_t xHigherPriorityWoken = pdFALSE;
		xQueueSendToBackFromISR(queueLine,&val1,&xHigherPriorityWoken);
	}

	void PIN_INT2_IRQHandler() {
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH2);
		int val2 = 3;
		BaseType_t xHigherPriorityWoken = pdFALSE;
		xQueueSendToBackFromISR(queueLine,&val2,&xHigherPriorityWoken);
	}
}

static void vReadTask(void *pvParameters) {
	char debug[30];
	int getNum;
	int lastValue;
	int number1 = 0;
	int number2 = 0;
	int number3 = 0;
		while (1) {
			xQueueReceive(queueLine,&getNum,portMAX_DELAY);
			switch(lastValue)
			{
				case 1 :
					if(getNum != lastValue)
					{
						sprintf(debug,"Button 1 pressed %d times.\n", number1);
						ITM_write(debug);
						number1 = 0;
					}
					number1++;
					break;
				case 2 :
					if(getNum != lastValue)
					{
						sprintf(debug,"Button 2 pressed %d times.\n", number2);
						ITM_write(debug);
						number2 = 0;
					}
					number2++;
					break;
				case 3 :
					if(getNum != lastValue)
					{
						sprintf(debug,"Button 3 pressed %d times.\n", number3);
						ITM_write(debug);
						number3 = 0;
					}
					number3++;
					break;
			}

			lastValue = getNum;

			vTaskDelay(10);
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

	xTaskCreate(vReadTask, "vTaskRead",
				configMINIMAL_STACK_SIZE *3 , NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	vTaskStartScheduler();


	return 1;
}

