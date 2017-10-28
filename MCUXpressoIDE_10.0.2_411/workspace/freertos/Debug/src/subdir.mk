################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/FreeRTOSCommonHooks.c \
../src/croutine.c \
../src/event_groups.c \
../src/heap_3.c \
../src/list.c \
../src/port.c \
../src/queue.c \
../src/tasks.c \
../src/timers.c 

OBJS += \
./src/FreeRTOSCommonHooks.o \
./src/croutine.o \
./src/event_groups.o \
./src/heap_3.o \
./src/list.o \
./src/port.o \
./src/queue.o \
./src/tasks.o \
./src/timers.o 

C_DEPS += \
./src/FreeRTOSCommonHooks.d \
./src/croutine.d \
./src/event_groups.d \
./src/heap_3.d \
./src/list.d \
./src/port.d \
./src/queue.d \
./src/tasks.d \
./src/timers.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__CODE_RED -DCORE_M3 -D__USE_LPCOPEN -DCR_INTEGER_PRINTF -DCR_PRINTF_CHAR -D__LPC15XX__ -D__NEWLIB__ -I"C:\Users\Nick\Documents\MCUXpressoIDE_10.0.2_411\workspace\freertos\inc" -I"C:\Users\Nick\Documents\MCUXpressoIDE_10.0.2_411\workspace\lpc_board_nxp_lpcxpresso_1549\inc" -I"C:\Users\Nick\Documents\MCUXpressoIDE_10.0.2_411\workspace\lpc_chip_15xx\inc" -O0 -fno-common -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -mcpu=cortex-m3 -mthumb -D__NEWLIB__ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


