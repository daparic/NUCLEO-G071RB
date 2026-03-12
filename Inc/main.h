/**
 ******************************************************************************
 * @file    main.h
 * @brief   Header for main.c
 ******************************************************************************
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g0xx_hal.h"

/* LED4 = PA5 (green LED on NUCLEO-G071RB) */
#define LED4_PIN            GPIO_PIN_5
#define LED4_GPIO_PORT      GPIOA

/* USER button = PC13 */
#define USER_BUTTON_PIN     GPIO_PIN_13
#define USER_BUTTON_PORT    GPIOC

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
