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
#include <stdlib.h>
#include <stdio.h>

//#include <iostream>
#include <string.h>

#define PWM_freq 1000 //Hz
#define PWM_cycle 50 //%

// TODO: insert other definitions and declarations here

//SemaphoreHandle_t countingSemaphore;
//SemaphoreHandle_t backQuestion;


void SCT_Init(void)
{
	LPC_SCT0->CONFIG |= (1 << 0) | (1 << 17);			// two 16-bit timers, auto limit
	LPC_SCT0->CTRL_L |= (72-1) << 5; 					// set prescaler, SCTimer/PWM clock = 1 MHz

	LPC_SCT0->MATCHREL[0].L = PWM_freq -1 ;				// match 0 @ 1000/1MHz = (1 kHz PWM freq)
	LPC_SCT0->MATCHREL[1].L = PWM_cycle;				// match 1 used for duty cycle (in 10 steps)-5%
	LPC_SCT0->MATCHREL[2].L = PWM_cycle;				// match 1 used for duty cycle (in 10 steps)-5%

	LPC_SCT0->EVENT[0].STATE = 0xFFFFFFFF; 				// event 0 happens in all states
	LPC_SCT0->EVENT[0].CTRL = (1 << 12); 				// match 0 condition only
	LPC_SCT0->EVENT[1].STATE = 0xFFFFFFFF; 				// event 1 happens in all states
	LPC_SCT0->EVENT[1].CTRL = (1 << 0) | (1 << 12);		// match 1 condition only
	LPC_SCT0->EVENT[2].STATE = 0xFFFFFFFF; 				// event 1 happens in all states
	LPC_SCT0->EVENT[2].CTRL = (2 << 0) | (1 << 12);		// match 1 condition only

	LPC_SCT0->OUT[0].SET = (1 << 0); 					// event 0 will set SCTx_OUT0
	LPC_SCT0->OUT[0].CLR = (1 << 1); 					// event 1 will clear SCTx_OUT0
	LPC_SCT0->OUT[1].SET = (1 << 0); 					// event 0 will set SCTx_OUT0
	LPC_SCT0->OUT[1].CLR = (1 << 2); 					// event 1 will clear SCTx_OUT0

	LPC_SCT0->CTRL_L &= ~(1 << 2); 						// unhalt it by clearing bit 2 of CTRL reg

	LPC_SCT1->CONFIG |= (1 << 17); 						// two 16-bit timers, auto limit
	LPC_SCT1->CTRL_L |= (72-1) << 5; 					// set prescaler, SCTimer/PWM clock = 1 MHz
	LPC_SCT1->MATCHREL[0].L = PWM_freq -1 ;				// match 0 @ 1000/1MHz = (1 kHz PWM freq)
	LPC_SCT1->MATCHREL[1].L = PWM_cycle;				// match 1 used for duty cycle (in 10 steps)-5%
	LPC_SCT1->EVENT[0].STATE = 0xFFFFFFFF; 				// event 0 happens in all states
	LPC_SCT1->EVENT[0].CTRL = (1 << 12); 				// match 0 condition only
	LPC_SCT1->EVENT[1].STATE = 0xFFFFFFFF; 				// event 1 happens in all states
	LPC_SCT1->EVENT[1].CTRL = (1 << 0) | (1 << 12);		// match 1 condition only
	LPC_SCT1->OUT[0].SET = (1 << 0); 					// event 0 will set SCTx_OUT0
	LPC_SCT1->OUT[0].CLR = (1 << 1); 					// event 1 will clear SCTx_OUT0
	LPC_SCT1->CTRL_L &= ~(1 << 2); 						// unhalt it by clearing bit 2 of CTRL reg
}

/*void SCT_Init2(void)
{
	LPC_SCT2->CONFIG |= (1 << 17); 						// two 16-bit timers, auto limit
	LPC_SCT2->CTRL_L |= (72-1) << 5; 					// set prescaler, SCTimer/PWM clock = 1 MHz
	LPC_SCT2->MATCHREL[0].L = PWM_freq -1 ;				// match 0 @ 1000/1MHz = (1 kHz PWM freq)
	LPC_SCT2->MATCHREL[1].L = PWM_cycle;				// match 1 used for duty cycle (in 10 steps)-5%
	LPC_SCT2->EVENT[0].STATE = 0xFFFFFFFF; 				// event 0 happens in all states
	LPC_SCT2->EVENT[0].CTRL = (1 << 12); 				// match 0 condition only
	LPC_SCT2->EVENT[1].STATE = 0xFFFFFFFF; 				// event 1 happens in all states
	LPC_SCT2->EVENT[1].CTRL = (1 << 0) | (1 << 12);		// match 1 condition only
	LPC_SCT2->OUT[0].SET = (1 << 0); 					// event 0 will set SCTx_OUT0
	LPC_SCT2->OUT[0].CLR = (1 << 1); 					// event 1 will clear SCTx_OUT0
	LPC_SCT2->CTRL_L &= ~(1 << 2); 						// unhalt it by clearing bit 2 of CTRL reg
}*/

static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();
	ITM_init();
	Chip_SCT_Init(LPC_SCT0);
	Chip_SCT_Init(LPC_SCT1);
	SCT_Init();
	//Chip_SCT_Init(LPC_SCT2);
	//SCT_Init2();


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


/* LED1 toggle thread */
static void receive_task(void *pvParameters) {

	//char inChar;
	bool detectrgb = false;

	char strRe[RCV_BUFSIZE];
	char typeIn[60] = "\n";
	const char rgbKey[] = "rgb";
	char *slice;
	char *hexStr;
	char *pEnd;
	char strDecimal[10];
	char colorStr[35];
	char debug[60];

	int  strPos = 0;
	long int decimal = 0;
	long int red;
	long int blue;
	long int green;
	float ratio_r;
	float ratio_g;
	float ratio_b;
	int percent = 0;

	std::string str = "0x";


	//USB_send((uint8_t *)str1, sizeof(str1));

	USB_send((uint8_t *)"\r\n Start :\n", 11);
	char start[50] = "\r\nStart by writing: rgb#+6 hex digits \r\n";
	USB_send((uint8_t*)start, 50);

	while (1) {
		uint32_t len = USB_receive((uint8_t *)strRe, RCV_BUFSIZE);
		strRe[len] = 0;
		//ITM_write(strRe);
		USB_send((uint8_t*)strRe, len);
		/*for(uint32_t i=0; i < len; i++){
			if(strRe[0] == "r" && strRe[1] == "g" && strRe[2] == "b"){
				detectrgb = true;
			}*/
		if (len > 0)
		{
			if((len == 60) || (strRe[0] == 13)  ){
				USB_send((uint8_t *)"\r\n", 2);
				typeIn[strPos] = 0;
				typeIn[strPos+1] = 10;
				typeIn[strPos+2] = 13;
				ITM_write(typeIn);
				USB_send((uint8_t *)typeIn, strPos + 3);
				strPos = 0;

				slice = strtok(typeIn, "# \r\n");
				if (strcmp (slice, rgbKey) == 0)
				{
					//get hexadecimal string and slice # away
					hexStr = strtok(NULL,"# \r\n");
					//USB_send((uint8_t*)hexStr, 7);

					//adding 0x to string
					str = "0x";
					str.append(hexStr);
					//str0x = str;

					char *cstr = new char[str.length() + 1];
					strcpy(cstr, str.c_str());
					// print string and convert to decimal
					USB_send((uint8_t*)cstr, str.size() +1);
					USB_send((uint8_t *)"\r\n", 2);
					decimal = strtol(cstr,&pEnd,16);
					delete [] cstr;

					//calculating rgb value
					red = decimal / (65536);
					green = (decimal - (red * 65536)) / 256;
					blue = decimal - (red * 65536 + green * 256);


					sprintf(strDecimal,"\r\n%ld \r\n",decimal);
					USB_send((uint8_t*)strDecimal, sizeof(strDecimal));

					sprintf(colorStr,"\r\nred: %ld, green: %ld, blue: %ld \r\n",red,green,blue);
					USB_send((uint8_t*)colorStr, sizeof(colorStr));
					USB_send((uint8_t *)"\r\n", 2);

					//get rgb ratio
					ratio_r = 1 -((float)red / 255);
					ratio_g = 1- ((float)green / 255);
					ratio_b = 1 - ((float)blue / 255);

					//implement new match duty cycle value
					LPC_SCT0->MATCHREL[1].L = (1000*ratio_r);

					sprintf(debug, "Duty cycle: %d\n", LPC_SCT0->MATCHREL[1].L);
					ITM_write(debug);

					Chip_SWM_MovablePortPinAssign(SWM_SCT0_OUT0_O, 0, 25);

					//-------------------------------------------------------

					LPC_SCT0->MATCHREL[2].L = (1000*ratio_g);

					sprintf(debug, "Duty cycle: %d\n", LPC_SCT0->MATCHREL[2].L);
					ITM_write(debug);

					Chip_SWM_MovablePortPinAssign(SWM_SCT0_OUT1_O, 0, 3);

					//-------------------------------------------------------

					LPC_SCT1->MATCHREL[1].L = (1000*ratio_b);

					sprintf(debug, "Duty cycle: %d\n", LPC_SCT2->MATCHREL[1].L);
					ITM_write(debug);

					Chip_SWM_MovablePortPinAssign(SWM_SCT1_OUT0_O, 1, 1);

				}
			}
			else
			{
				typeIn[strPos] = strRe[0];
				strPos++;
				if (strPos > 50) strPos = 50;
			}

		}
		vTaskDelay(1);

	}

}



int main(void) {

	prvSetupHardware();

	/* LED1 toggle thread */
	xTaskCreate(receive_task, "Rx",
				configMINIMAL_STACK_SIZE *4, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);

	/* LED2 toggle thread */
	xTaskCreate(cdc_task, "CDC",
				configMINIMAL_STACK_SIZE *2, NULL, (tskIDLE_PRIORITY + 1UL),
				(TaskHandle_t *) NULL);



	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}
