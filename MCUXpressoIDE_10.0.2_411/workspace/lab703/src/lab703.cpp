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
#include "sct_15xx.h"
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

//LPC_SCT_T *LPC_SCT;


#define PWM_freq 1000 //Hz
#define PWM_cycle 50 //%

SemaphoreHandle_t hitSW;

void SCT_Init(void)
{

	LPC_SCT0->CONFIG |= (1 << 17); 						// two 16-bit timers, auto limit
	LPC_SCT0->CTRL_L |= (72-1) << 5; 					// set prescaler, SCTimer/PWM clock = 1 MHz
	LPC_SCT0->MATCHREL[0].L = PWM_freq -1 ;				// match 0 @ 1000/1MHz = (1 kHz PWM freq)
	LPC_SCT0->MATCHREL[1].L = PWM_cycle;				// match 1 used for duty cycle (in 10 steps)-5%
	LPC_SCT0->EVENT[0].STATE = 0xFFFFFFFF; 				// event 0 happens in all states
	LPC_SCT0->EVENT[0].CTRL = (1 << 12); 				// match 0 condition only
	LPC_SCT0->EVENT[1].STATE = 0xFFFFFFFF; 				// event 1 happens in all states
	LPC_SCT0->EVENT[1].CTRL = (1 << 0) | (1 << 12);		// match 1 condition only
	LPC_SCT0->OUT[0].SET = (1 << 0); 					// event 0 will set SCTx_OUT0
	LPC_SCT0->OUT[0].CLR = (1 << 1); 					// event 1 will clear SCTx_OUT0
	LPC_SCT0->CTRL_L &= ~(1 << 2); 						// unhalt it by clearing bit 2 of CTRL reg
	//Chip_SCTPWM_SetDutyCycle(LPC_SCT0, 1, Chip_SCTPWM_PercentageToTicks(LPC_SCT0,	PWM_cycle));
}

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();
	ITM_init();
	Chip_SCT_Init(LPC_SCT0);
	SCT_Init();


	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}

extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
	}
}

static void ReadUSBTask(void *pvParameters) {
	char buffer[60];

	while(1){


	}
}


//Debug print task
static void vDutyCycleTask(void *pvParameters) {
	DigitalIoPin button1 (17, 0, true, true, false);
	DigitalIoPin button3 (9, 1, true, true, false);
	DigitalIoPin button2 (11, 1, true, true, false);

	//int pressVal = 0; // when release
	bool button1press;
	bool button3press;
	bool button2press;
	bool button1release = true;
	bool button3release = true;

	int duty_cycle = PWM_cycle;
	//int last_value = duty_cycle;
	char debug[60];
	int percent = 0;


		while (1) {
			//read button1,2,3
			button1press = button1.read();
			button2press = button2.read();
			button3press = button3.read();

			if (duty_cycle < 2){duty_cycle = 0;}
			if (duty_cycle > 998){duty_cycle = 1000;}

			if (button1press == true ){
				if (button1release) {
					duty_cycle = duty_cycle + 20;
				}
				button1release = false;
			}else{button1release = true;}

			if (button3press == true ){
				if (button3release) {
					duty_cycle = duty_cycle - 20;
				}
				button3release = false;
			}else{button3release = true;}


			if (button1press == true && button2press == true){
				duty_cycle = duty_cycle*5;
			}
			if (button3press == true && button2press == true){
				duty_cycle = duty_cycle/5;
			}



			//Update duty cycle
			LPC_SCT0->MATCHREL[1].L = duty_cycle;

			percent = ((1000 - (int)LPC_SCT0->MATCHREL[1].L)/10);

			sprintf(debug, "Duty cycle: %d\n", percent);
			if(percent > -1 && percent < 101){ITM_write(debug);}

			Chip_SWM_MovablePortPinAssign(SWM_SCT0_OUT0_O, 0, 3);

			vTaskDelay(100);

		}
}

int main(void)
{
	prvSetupHardware();
	hitSW = xSemaphoreCreateBinary();


	ITM_write("ITM started \r\n");

	xTaskCreate(ReadUSBTask, "ReadUSBTask",
						configMINIMAL_STACK_SIZE * 3 , NULL, (tskIDLE_PRIORITY + 1UL),
						(TaskHandle_t *) NULL);

	xTaskCreate(vDutyCycleTask, "vDutyCycleTask",
					configMINIMAL_STACK_SIZE * 3 , NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);


	vTaskStartScheduler();


	return 1;
}

