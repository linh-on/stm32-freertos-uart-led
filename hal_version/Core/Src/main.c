#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

UART_HandleTypeDef huart3;

#define POSITIVE 1
#define NEGATIVE 0

volatile int g_mode = POSITIVE;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);

void LEDTask(void *argument);
void ButtonTask(void *argument);
void UartTask(void *argument);

void Error_Handler(void);


int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART3_UART_Init();

    xTaskCreate(LEDTask,    "LED",    128, NULL, 2, NULL);
    xTaskCreate(ButtonTask, "BUTTON", 128, NULL, 1, NULL);
    xTaskCreate(UartTask,   "UART",   256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) { }
}


void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    // HSI on, no PLL, 16 MHz SYSCLK and PCLK1
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK |
                                       RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 |
                                       RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}


static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // PB0 heartbeat LED, PB7 button indicator LED
    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7, GPIO_PIN_RESET);

    // PC13 USER button B1
    GPIO_InitStruct.Pin  = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // PD8 USART3 TX, PD9 USART3 RX
    GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}


static void MX_USART3_UART_Init(void)
{
    __HAL_RCC_USART3_CLK_ENABLE();

    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 9600;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
}


void LEDTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void ButtonTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        GPIO_PinState button = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
        GPIO_PinState led;

        if (g_mode == POSITIVE)
        {
            led = (button == GPIO_PIN_SET) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        }
        else
        {
            led = (button == GPIO_PIN_RESET) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        }

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, led);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void UartTask(void *argument)
{
    (void)argument;
    uint8_t ch;

    for (;;)
    {
        if (HAL_UART_Receive(&huart3, &ch, 1, 1000) == HAL_OK)
        {
            if (ch == 'p')
            {
                g_mode = POSITIVE;
                HAL_UART_Transmit(&huart3, &ch, 1, HAL_MAX_DELAY);
            }
            else if (ch == 'n')
            {
                g_mode = NEGATIVE;
                HAL_UART_Transmit(&huart3, &ch, 1, HAL_MAX_DELAY);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}


void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}