/**
 ******************************************************************************
 * @file    main.c
 * @brief   Blinking LED demo for NUCLEO-G071RB.
 *
 * LED4 (PA5) blinks at a rate that increases each time the USER button
 * (PC13) is pressed.  The blink periods cycle through:
 *   1000 ms → 500 ms → 250 ms → 125 ms → 62 ms → (wraps back to 1000 ms)
 *
 * State is reported over USART2 (PA2 TX) at 115200 baud, visible on the
 * host as /dev/ttyACM0 via the ST-LINK Virtual COM Port.
 ******************************************************************************
 */

#include "main.h"
#include "stm32g0xx_it.h"

/* Blink half-periods in milliseconds (LED on for N ms, then off for N ms) */
static const uint32_t blink_periods[] = {1000, 500, 250, 125, 62};
#define BLINK_STEPS  (sizeof(blink_periods) / sizeof(blink_periods[0]))

/* Index into blink_periods[], updated in the button ISR */
volatile uint32_t blink_index = 0;

/* Set to 1 by the button ISR; cleared by the main loop after printing */
volatile uint8_t button_pressed = 0;

UART_HandleTypeDef huart2;

static void SystemClock_Config(void);
static void LED_Init(void);
static void Button_EXTI_Init(void);
static void USART2_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    USART2_Init();
    Button_EXTI_Init();

    printf("\r\n== NUCLEO-G0B1RE Blink+Button ==\r\n");
    printf("Press USER button to increase blink speed.\r\n");
    printf("[BOOT] speed=%lu  half-period=%lu ms\r\n",
           blink_index, blink_periods[blink_index]);

    while (1)
    {
        /* Deferred print: button_pressed is set in the ISR, printed here */
        if (button_pressed)
        {
            button_pressed = 0;
            printf("[BTN]  speed=%lu  half-period=%lu ms\r\n",
                   blink_index, blink_periods[blink_index]);
        }

        uint32_t half = blink_periods[blink_index];
        HAL_GPIO_WritePin(LED4_GPIO_PORT, LED4_PIN, GPIO_PIN_SET);
        HAL_Delay(half);
        HAL_GPIO_WritePin(LED4_GPIO_PORT, LED4_PIN, GPIO_PIN_RESET);
        HAL_Delay(half);
    }
}

/**
 * @brief  Configure system clock to 64 MHz using HSI + PLL.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv              = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN            = 8;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief  Initialise LED4 (PA5) as push-pull output, initially off.
 */
static void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(LED4_GPIO_PORT, LED4_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LED4_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED4_GPIO_PORT, &GPIO_InitStruct);
}

/**
 * @brief  Initialise USART2 at 115200 8N1 on PA2 (TX) / PA3 (RX).
 *         PA2/PA3 are wired to the ST-LINK VCP, which appears as
 *         /dev/ttyACM0 on the host.
 */
static void USART2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA2 = USART2_TX, PA3 = USART2_RX  (AF1 on STM32G071) */
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief  Configure USER button (PC13) as falling-edge EXTI interrupt.
 */
static void Button_EXTI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin  = USER_BUTTON_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USER_BUTTON_PORT, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

/**
 * @brief  EXTI callback — advance to the next (faster) blink speed.
 *
 * Runs in ISR context: only update state here.
 * The main loop detects button_pressed and calls printf safely.
 */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == USER_BUTTON_PIN)
    {
        blink_index = (blink_index + 1) % BLINK_STEPS;
        button_pressed = 1;
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
