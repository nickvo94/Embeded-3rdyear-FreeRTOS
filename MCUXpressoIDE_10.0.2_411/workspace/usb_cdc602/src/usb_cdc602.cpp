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
#include "DigitalIOPin.h"
#include <queue.h>

#include <mutex>
#include "Fmutex.h"
#include "user_vcom.h"
#include <sstream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
// TODO: insert other definitions and declarations here

volatile uint32_t RIT_count;
xSemaphoreHandle sbRIT;

SemaphoreHandle_t stop;
SemaphoreHandle_t go;
SemaphoreHandle_t taskComplete;
SemaphoreHandle_t left;
SemaphoreHandle_t right;
SemaphoreHandle_t move;
SemaphoreHandle_t hitSW;
SemaphoreHandle_t hitBothSW;
QueueSetHandle_t queueLine;
QueueSetHandle_t queueUsbReceive;


enum direction_enum {STOP, LEFT, RIGHT};
volatile uint32_t  pulsePS = 400;
struct motorTask_t
{
	direction_enum  direction;
	uint32_t        amount;
	uint32_t		pps;
};

struct usb_receive_string_t
{
	char			data[10];
	uint32_t		length;
};

const char keyLeft[] = "left";
const char keyRight[] = "right";
const char keyPulse[] = "pps";
const char keyStop[] = "stop";
const char keyGo[] = "go";
const char leftRep[20] = "\r\nleft received\r\n";
const char stopRep[20] = "\r\nstop received\r\n";
const char rightRep[20] = "\r\nright received\r\n";
const char PulseRep[20] = "\r\npulse change \r\n";
const char QfullRep[20] = "\r\nQueue Full\r\n";

char usb_receive_buffer[10];

/* the following is required if runtime statistics are to be collected */
extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(256) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
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
	// initialize RIT (= enable clocking etc.)
	Chip_RIT_Init(LPC_RITIMER);
	// set the priority level of the interrupt
	// The level must be equal or lower than the maximum priority specified in FreeRTOS config
	// Note that in a Cortex-M3 a higher number indicates lower interrupt priority
	NVIC_SetPriority( RITIMER_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1 );
}

static void moveTask(void *pvParameters){
	DigitalIoPin portStep (24, 0, false, true, false);
	DigitalIoPin portDir (0, 1, false, true, false);

	bool direction = true;
	int begin = 0;
	int trialTime = 0;
	int countStep = 0;
	int stepArray [3];
	int averageStep = 0;
	int runningStep = 0;
	char dir_debug[80];

	while(1){
//----------------------------------------------------------------------------
		while (begin == 0) {
			if(xSemaphoreTake(hitBothSW, 1) != pdPASS){
				//ITM_write(" Receive not hitBothSW .\n ");

				portDir.write(direction);
				portStep.write(true);
				vTaskDelay(1);
				portStep.write(false);

				countStep++;

				if(xSemaphoreTake(hitSW, 1) == pdPASS){
					//change direction
					sprintf(dir_debug, "Direction: %d , trial time : %d, count step : %d\n", (int)direction, trialTime, countStep);
					ITM_write(" Receive hitSW .\n ");
					ITM_write(dir_debug);

					direction = !direction;
					portDir.write(direction);

					//start to collect step number from 2nd trial
					if(trialTime >0){stepArray[trialTime-1]= countStep;}
					countStep = 0;
					trialTime ++;
					//DEBUGOUT("Trial time : %d .\r\n ", trialTime);

				}
				//ITM_write("Out of loop .\n ");


				// Calculating average step
				if(trialTime == 4){
					trialTime = 0;
					averageStep = (stepArray[0] + stepArray [1] + stepArray [2])/3;
					averageStep = averageStep - 20;
					begin = 1;

					Board_LED_Set(2, true);
					vTaskDelay(2000);
				}
				//vTaskDelay(1);

			}else{ITM_write("Delay.\n");vTaskDelay(5000);}


		}
		Board_LED_Set(2, false);
		vTaskDelay(1);
//-------------------------------------------------------------------------------------
		//Running to the middle
		//bool direction2 = true;
		while(begin == 1 ){
			runningStep++;
			portDir.write(direction);

			portStep.write(true);
			vTaskDelay(1);
			portStep.write(false);
			//vTaskDelay(1);

			if(runningStep == (averageStep / 2)){
				portStep.write(false);
				//runningStep = 20;
				begin = 2;
			}
		}
		vTaskDelay(1);
//--------------------------------------------------

		xSemaphoreGive(taskComplete);		// Calibration ends, give a taskComplete to enable the task queue.

		while(begin == 2){
			if(xSemaphoreTake(left,1) == pdPASS){portDir.write(true);}
			if(xSemaphoreTake(right,1) == pdPASS){portDir.write(false);}
			if(xSemaphoreTake(hitSW,1)){portStep.write(false);RIT_count = 0;}

			if(xSemaphoreTake(move, 1)== pdPASS){
				ITM_write("Enter Move task \r\n");
				//portStep.write(true);
				//portStep.write(false);
			}else{
				//portStep.write(false);
			}
			vTaskDelay(1);
		}
//---------------------------------------------------
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
			if (sw1press == true && sw2press == true ){	xSemaphoreGive(hitBothSW);}
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

extern "C" {
void RIT_IRQHandler(void){
	DigitalIoPin portStep (24, 0, false, true, false);
	DigitalIoPin portDir (0, 1, false, true, false);
	// This used to check if a context switch is required
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	// Tell timer that we have processed the interrupt.
	// Timer then removes the IRQ until next match occurs
	Chip_RIT_ClearIntStatus(LPC_RITIMER); // clear IRQ flag
	if(RIT_count > 0) {
		RIT_count--;
		// do something useful here...
		//xSemaphoreGive(move);

		portStep.write(true);
		portStep.write(false);

		}
		else {
			//portStep.write(false);
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
	char debugStr[80];
	uint64_t cmp_value;

	ITM_write("Timer start \r\n");

	// Determine approximate compare value based on clock rate and passed interval
	cmp_value = (uint64_t) Chip_Clock_GetSystemClockRate() * (uint64_t) us / 1000000;
	sprintf(debugStr, "cmp_value = %d \r\n", (uint32_t)cmp_value);
	ITM_write(debugStr);
	// disable timer during configuration
	Chip_RIT_Disable(LPC_RITIMER);
	RIT_count = count;
	// enable automatic clear on when compare value==timer value
	// this makes interrupts trigger periodically
	Chip_RIT_EnableCompClear(LPC_RITIMER);
	// reset the counter
	ITM_write("Timer SetCounter timer \r\n");
	Chip_RIT_SetCounter(LPC_RITIMER, 0);
	Chip_RIT_SetCompareValue(LPC_RITIMER, cmp_value);
	// start counting
	ITM_write("Timer Enable \r\n");
	Chip_RIT_Enable(LPC_RITIMER);
	// Enable the interrupt signal in NVIC (the interrupt controller)
	ITM_write("Timer EnableIRQ \r\n");
	NVIC_EnableIRQ(RITIMER_IRQn);
	// wait for ISR to tell that we're done
	ITM_write("Wait for ISR SemaphoreTake \r\n");

	if(xSemaphoreTake(sbRIT, portMAX_DELAY) == pdTRUE) {
		// Disable the interrupt signal in NVIC (the interrupt controller)
		ITM_write("Received ISR SemaphoreTake, disable timer. \r\n");
		NVIC_DisableIRQ(RITIMER_IRQn);
		ITM_write("Disabled.\r\n");
		xSemaphoreGive(taskComplete);

	}
	else {
		// unexpected error
		ITM_write("Error happens \r\n");
	}

	ITM_write("Exit RIT start \r\n");

}

/* LED1 toggle thread */
static void receive_task(void *pvParameters)
{
	//char inChar;
	//char str2[5] = "\r\n";

	usb_receive_string_t 	usb_buffer;
	char 					typeIn[60] = "\n";
	char 					getStr[60] = "\n";

	char debug[80];

	char *slice;
	char *goSlice;
	char *number;
	char *pulse;
	int  strPos = 0;
	//int  pps    = 400;

	motorTask_t* createTask;
	//motorTask_t* readTask;

	ITM_write("ReceiveTask/Queuehandler started.\r\n");

	USB_send((uint8_t *)"\r\n Start :\n", 11);
	char start[50] = "\r\nStart enter command and type go: \r\n";
	USB_send((uint8_t*)start, 50);

	while (1) {

		usb_buffer.length = USB_receive((uint8_t *)usb_buffer.data, 10);
		if (usb_buffer.length > 0)
		{
			ITM_write(usb_buffer.data);
			USB_send((uint8_t*)usb_buffer.data, usb_buffer.length);
			if(usb_buffer.data[0] == 13)
			{
				USB_send((uint8_t *)"\r\n", 2);
				typeIn[strPos] = 0;
				typeIn[strPos+1] = 10;
				typeIn[strPos+2] = 13;
				ITM_write(typeIn);
				USB_send((uint8_t *)typeIn, strPos + 3);
				strPos = 0;

				slice = strtok(typeIn, " \r\n");
				if( strcmp (slice, keyGo) == 0)
				{
					USB_send((uint8_t *)"Go!\r\n", 5);
					//goForTasking = true;
					xSemaphoreGive(go);
					//delete createTask;
				}
				else
				{
					if( strcmp (slice, keyPulse) == 0){
						pulse = strtok(NULL," \r\n");
						pulsePS = atoi(pulse);
						//createTask = new motorTask_t;
						//createTask->pps = atoi(pulse);

						//sprintf(debug, "New: Pulse: %d \r\n", createTask->pps);
						//ITM_write(debug);

						//xQueueSendToBack(queueLine, &createTask, portMAX_DELAY);
						//if ( uxQueueSpacesAvailable(queueLine) == 0 ) USB_send((uint8_t *)QfullRep, 20);
					}
					if( strcmp (slice, keyLeft) == 0){
						number = strtok(NULL, " \r\n");
						createTask = new motorTask_t;
						createTask->direction = LEFT;
						createTask->amount    = atoi(number);

						sprintf(debug, "New: Dir: %d, Amount: %d \r\n", createTask->direction, createTask->amount);
						ITM_write(debug);

						xQueueSendToBack(queueLine, &createTask, portMAX_DELAY);
						if ( uxQueueSpacesAvailable(queueLine) == 0 ) USB_send((uint8_t *)QfullRep, 20);
					}
					if( strcmp (slice, keyRight) == 0){
						number = strtok(NULL, " \r\n");
						createTask = new motorTask_t;
						createTask->direction = RIGHT;
						createTask->amount    = atoi(number);

						sprintf(debug, "New: Dir: %d, Amount: %d \r\n", createTask->direction, createTask->amount);
						ITM_write(debug);

						xQueueSendToBack(queueLine, &createTask, portMAX_DELAY);
						if ( uxQueueSpacesAvailable(queueLine) == 0 ) USB_send((uint8_t *)QfullRep, 20);
					}
					if( strcmp (slice, keyStop) == 0){
						xSemaphoreGive(stop);
					}
				}
			}
			else
			{
				typeIn[strPos] = usb_buffer.data[0];
				strPos++;
				if (strPos > 50) strPos = 50;
			}
		}

	}
	vTaskDelay(1);
}

static void ExecuteQTask(void *pvParameters)
{
	char debug[80];


	bool timerAvailable = false;
	bool goForTasking   = false;

	motorTask_t* readTask;

	while(1)
	{
		if (xSemaphoreTake(taskComplete, 1))
			{
			timerAvailable = true;
			}

		stopHere:

		if (xSemaphoreTake(go, 1))
			{
				goForTasking = true;
			}

		if (goForTasking && timerAvailable)
		{
			if (xSemaphoreTake(stop, 1) == pdPASS)
			{
				USB_send((uint8_t *)stopRep, sizeof(stopRep));
				goForTasking = false;
				goto stopHere;
			}
			ITM_write("queue: taskComplete = true\r\n");
			if (uxQueueMessagesWaiting(queueLine) < 1)
			{
				ITM_write("queue: Queue empty.\r\n");
				goForTasking = false;

			}
			else
			{
				if (xQueueReceive(queueLine, &readTask, 1) == pdPASS)
				{
					sprintf(debug, "queue: Got item: Dir: %d, Amount: %d , Pulse: %d \r\n", readTask->direction, readTask->amount, pulsePS);
					ITM_write(debug);
					timerAvailable = false;

					if (readTask->direction == RIGHT)
					{
						USB_send((uint8_t *)rightRep, sizeof(rightRep));
						xSemaphoreGive(right);
						RIT_start(readTask->amount, (1000000/(2*pulsePS)));
					}
					if (readTask->direction == LEFT)
					{
						USB_send((uint8_t *)leftRep, sizeof(leftRep));
						xSemaphoreGive(left);
						RIT_start(readTask->amount, (1000000/(2*pulsePS)));
					}

					delete readTask;
					vTaskDelay(1);
				}
				else
				{

					ITM_write("queue: Error reading queue.\r\n");
				}
			}
		}

	}

}

int main(void) {

	prvSetupHardware();
	ITM_init();

	taskComplete = xSemaphoreCreateBinary();
	left 		 = xSemaphoreCreateBinary();
	right 		 = xSemaphoreCreateBinary();
	move 		 = xSemaphoreCreateBinary();
	hitSW 		 = xSemaphoreCreateBinary();
	hitBothSW 	 = xSemaphoreCreateBinary();
	go	 		 = xSemaphoreCreateBinary();
	stop 		 = xSemaphoreCreateBinary();

	queueLine 		= xQueueCreate(20, sizeof(motorTask_t*));
	queueUsbReceive = xQueueCreate(2, sizeof(usb_receive_string_t*));

	sbRIT = xSemaphoreCreateBinary();

	xTaskCreate(ExecuteQTask, "TaskExecuteQ",
							configMINIMAL_STACK_SIZE * 4 , NULL, (tskIDLE_PRIORITY + 1UL),
							(TaskHandle_t *) NULL);

	xTaskCreate(vReadSWTask1, "vTask1ReadSW",
						configMINIMAL_STACK_SIZE * 2 , NULL, (tskIDLE_PRIORITY + 1UL),
						(TaskHandle_t *) NULL);

	xTaskCreate(moveTask, "MT",
					configMINIMAL_STACK_SIZE * 4, NULL, (tskIDLE_PRIORITY + 1UL),
					(TaskHandle_t *) NULL);

	xTaskCreate(receive_task, "Rx",
				configMINIMAL_STACK_SIZE * 4, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);


	xTaskCreate(cdc_task, "CDC",
				configMINIMAL_STACK_SIZE * 2, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();
	/* Should never arrive here */
	return 1;
}
