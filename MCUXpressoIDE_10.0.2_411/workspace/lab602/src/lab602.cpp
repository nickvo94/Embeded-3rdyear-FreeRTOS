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

volatile uint32_t RIT_count;
xSemaphoreHandle sbRIT;


SemaphoreHandle_t hitSW;
SemaphoreHandle_t hitBothSW ;

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

Syslog logger = Syslog();

extern "C" {
void RIT_IRQHandler(void){
	// This used to check if a context switch is required
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	// Tell timer that we have processed the interrupt.
	// Timer then removes the IRQ until next match occurs
	Chip_RIT_ClearIntStatus(LPC_RITIMER); // clear IRQ flag
	if(RIT_count > 0) {
		RIT_count--;
		// do something useful here...
	}
	else {
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
	uint64_t cmp_value;
	// Determine approximate compare value based on clock rate and passed interval
	cmp_value = (uint64_t) Chip_Clock_GetSystemClockRate() * (uint64_t) us / 1000000;
	// disable timer during configuration
	Chip_RIT_Disable(LPC_RITIMER);
	RIT_count = count;
	// enable automatic clear on when compare value==timer value
	// this makes interrupts trigger periodically
	Chip_RIT_EnableCompClear(LPC_RITIMER);
	// reset the counter
	Chip_RIT_SetCounter(LPC_RITIMER, 0);
	Chip_RIT_SetCompareValue(LPC_RITIMER, cmp_value);
	// start counting
	Chip_RIT_Enable(LPC_RITIMER);
	// Enable the interrupt signal in NVIC (the interrupt controller)
	NVIC_EnableIRQ(RITIMER_IRQn);
	// wait for ISR to tell that we're done
	if(xSemaphoreTake(sbRIT, portMAX_DELAY) == pdTRUE) {
		// Disable the interrupt signal in NVIC (the interrupt controller)
		NVIC_DisableIRQ(RITIMER_IRQn);
	}
	else {
		// unexpected error
	}
}

static void vCommandTask(void *pvParameters) {


	while(1){
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

		if (sw1press == true && sw2press == true ){
			xSemaphoreGive(hitBothSW);
		}
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

//Debug print task
static void vMoveTask2(void *pvParameters) {
	//DigitalIoPin button1 (17, 0, true, true, false);
	//DigitalIoPin button3 (9, 1, true, true, false);
	DigitalIoPin portStep (24, 0, false, true, false);
	DigitalIoPin portDir (0, 1, false, true, false);

	while(1){
		vTaskDelay(1);
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
	RIT_IRQHandler();

	ITM_init();

	hitSW = xSemaphoreCreateBinary();
	hitBothSW = xSemaphoreCreateBinary();

	xTaskCreate(vCommandTask, "vTaskCommand",
				configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 2UL),
				(TaskHandle_t *) NULL);

	xTaskCreate(vReadSWTask1, "vTask1ReadSW",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(vMoveTask2, "vMoveTask2",
					configMINIMAL_STACK_SIZE +128 , NULL, (tskIDLE_PRIORITY + 2UL),
					(TaskHandle_t *) NULL);


	vTaskStartScheduler();


	return 1;
}

