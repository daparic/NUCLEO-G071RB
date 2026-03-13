/**
 ******************************************************************************
 * @file    stm32g0xx_it.c
 * @brief   Interrupt service routines.
 ******************************************************************************
 */

#include "main.h"
#include "stm32g0xx_it.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while (1)
    {
    }
}

void SVC_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/**
 * @brief  EXTI line 4..15 — handles USER button on PC13.
 */
void EXTI4_15_IRQHandler(void)
{
    /* In /home/dx/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.2/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_gpio.c
       which then makes a call to a weak HAL_GPIO_EXTI_Falling_Callback on same file stm32g0xx_hal_gpio.c and 
       which main.c here has override
    */ 
    HAL_GPIO_EXTI_IRQHandler(USER_BUTTON_PIN);
}
