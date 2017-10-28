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
#include <stdlib.h>

#include "ITM_write.h"

#include <string.h>
// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"

QueueSetHandle_t queueLine;

SemaphoreHandle_t learning;
SemaphoreHandle_t nothing;


int filterTime = 50;

struct swEvent{
	uint8_t sendVal;
	uint32_t ticks;
};

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

	NVIC_ClearPendingIRQ(PIN_INT0_IRQn);
	NVIC_ClearPendingIRQ(PIN_INT1_IRQn);
	NVIC_ClearPendingIRQ(PIN_INT2_IRQn);

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

void queueFunction(int i)
{
	swEvent e;
	e.sendVal = i;
	e.ticks = xTaskGetTickCountFromISR();
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	xQueueSendToBackFromISR(queueLine, &e, &xHigherPriorityWoken);
}

extern "C" {
	void PIN_INT0_IRQHandler() {
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
		int val = 0;
		queueFunction(val);
	}

	void PIN_INT1_IRQHandler() {
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);
		int val1 = 1;
		queueFunction(val1);
	}

	void PIN_INT2_IRQHandler() {
		Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH2);
		int val1 = 2;
		queueFunction(val1);
	}

}

static void vMonitorTask(void *pvParameters) {
	DigitalIoPin sw1(17,0, true, true, false);
	DigitalIoPin sw2(11,1, true, true, false);
	DigitalIoPin sw3(9, 1, true, true, false);

	int monitorTime = 0;
	int nothingTime = 0;
	bool sw1press;
	bool sw2press;
	bool sw3press;

	while(1){
		sw1press = sw1.read();
		sw2press = sw2.read();
		sw3press = sw3.read();

		if (sw3press == true ){
			monitorTime++;
			if(monitorTime == 3000){
				xSemaphoreGive(learning);
			}
		}else{
			monitorTime = 0;
		}


		if (sw1press == false && sw2press == false && sw3press == false ){
			nothingTime++;
		}else{nothingTime = 0;}

		if(nothingTime == 14000){
			xSemaphoreGive(nothing);
		}
		vTaskDelay(1);
	}
}

static void vReadTask(void *pvParameters) {
	char 	key[9]				= {1,1,1,1,0,1,1,0};
	char 	input_buffer[9]		= {255,255,255,255,255,255,255,255};
	int		input_buffer_pos	= 0;
	int 	prev_input_time 	= 0;
	int 	key_index 			= 0;
	char	debug[50];
	//int		ascii_conv_add		= 48;
	swEvent e;

	const 	int input_interval_ms  	= 50;

	while(true)
	{
		xQueueReceive(queueLine,&e,portMAX_DELAY);
		if (e.ticks - prev_input_time > input_interval_ms)
		{
			sprintf(debug, "input: %d    buffer: ", e.sendVal);
			ITM_write(debug);
			input_buffer[input_buffer_pos] = e.sendVal;

			bool match     = true;
			int  check_pos = input_buffer_pos + 1;

			for (int i = 0; i < 8; i++)
			{
				if (check_pos > 7) check_pos = 0;

				int val0 = (int)input_buffer[check_pos];
				int val1 = (int)key[i];

				sprintf(debug, "%d-%d   ", val0, val1);
				//sprintf(debug, "%d, ", val0);
				ITM_write(debug);

				if (val0 != val1) match = false;

				check_pos++;
			}

			ITM_write("\r\n");

			if (match)
			{
				ITM_write("match!\n");
				Board_LED_Set(0, false);
				Board_LED_Set(1, true);
				vTaskDelay(5000);
				Board_LED_Set(1, false);
				Board_LED_Set(0, true);
			}

			//learning mode
			if (xSemaphoreTake(learning,1) == pdPASS)
			{
				ITM_write("Start learning.\n");
				Board_LED_Set(0, false);
				Board_LED_Set(2, true);

				while(key_index < 8)
				{
					if (e.ticks - prev_input_time > input_interval_ms)
					{
						xQueueReceive(queueLine,&e,portMAX_DELAY);
						key[key_index] = e.sendVal;
						sprintf(debug, "%d, ", (int)key[key_index]);
						ITM_write(debug);
						key_index++;
					}
				}
				ITM_write("\r\n");
				key_index = 0;
				Board_LED_Set(2, false);
			}

			if (xSemaphoreTake(nothing,1) == pdPASS)memset(input_buffer, 225, 9);

			input_buffer_pos++;
			if (input_buffer_pos > 7) input_buffer_pos = 0;
			prev_input_time = e.ticks;
		}
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
	queueLine = xQueueCreate(8, sizeof(swEvent));
	learning = xSemaphoreCreateBinary();
	nothing = xSemaphoreCreateBinary();

	xTaskCreate(vMonitorTask, "vTaskMonitor",
						configMINIMAL_STACK_SIZE *3 , NULL, (tskIDLE_PRIORITY + 1UL),
						(TaskHandle_t *) NULL);

	xTaskCreate(vReadTask, "vTaskRead",
				configMINIMAL_STACK_SIZE *3 , NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	vTaskStartScheduler();


	return 1;
}

