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
#include <time.h>

// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>
#include "event_groups.h"
#include "timers.h"

#define mainONE_SHOT_TIMER_PERIOD 		( 20000 )
#define mainAUTO_RELOAD_TIMER_PERIOD 	( 5000 )

TimerHandle_t AutoReloadTimer;

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();
	/* Initial LED0 state is off */
	Board_LED_Set(1, false);
}

Syslog logger = Syslog();


//print task
static void vTask1(void *pvParameters) {
	DigitalIoPin button1 (17, 0, true, true, false);
	DigitalIoPin button2 (11, 1, true, true, false);
	DigitalIoPin button3 (9, 1, true, true, false);

	vTaskDelay(500);

	while(1)
	{
		if(button1.read() || button2.read() || button3.read())
		{
			Board_LED_Set(1, true);
			xTimerReset(AutoReloadTimer, portMAX_DELAY);
		}
		vTaskDelay(1);
	}

}

static void prvAutoReloadTimerCallback( TimerHandle_t xTimer )
{
	Board_LED_Set(1, false);
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


	/* Create the auto-reload timer, storing the handle to the created timer in xAutoReloadTimer. */
	AutoReloadTimer = xTimerCreate("Auto Reload", 5000,
									pdTRUE,	(void*)0, prvAutoReloadTimerCallback );

	xTaskCreate(vTask1, "vTask1",
				configMINIMAL_STACK_SIZE *4 , NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);


	vTaskStartScheduler();


	return 1;
}

