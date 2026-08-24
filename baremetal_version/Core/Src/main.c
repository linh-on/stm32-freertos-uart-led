#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

// RCC, base 0x40023800
#define RCC_AHB1ENR (*(volatile uint32_t*) (0x40023800 + 0x30))
#define RCC_APB1ENR (*(volatile uint32_t*) (0x40023800 + 0x40))

// GPIOB, base 0x40020400. PB0 heartbeat LED, PB7 button indicator LED.
#define GPIOB_MODER (*(volatile uint32_t*) (0x40020400 + 0x00))
#define GPIOB_ODR   (*(volatile uint32_t*) (0x40020400 + 0x14))

// GPIOC, base 0x40020800. PC13 USER button B1.
#define GPIOC_MODER (*(volatile uint32_t*) (0x40020800 + 0x00))
#define GPIOC_IDR   (*(volatile uint32_t*) (0x40020800 + 0x10))

// GPIOD, base 0x40020C00. PD8 USART3 TX, PD9 USART3 RX.
#define GPIOD_MODER (*(volatile uint32_t*) (0x40020C00 + 0x00))
#define GPIOD_AFRH  (*(volatile uint32_t*) (0x40020C00 + 0x24)) // pins 8 to 15

// USART3, base 0x40004800
#define USART3_SR   (*(volatile uint32_t*) (0x40004800 + 0x00))
#define USART3_DR   (*(volatile uint32_t*) (0x40004800 + 0x04))
#define USART3_BRR  (*(volatile uint32_t*) (0x40004800 + 0x08))
#define USART3_CR1  (*(volatile uint32_t*) (0x40004800 + 0x0C))

#define LED_HEARTBEAT 0
#define LED_BUTTON    7
#define BUTTON_PIN    13

#define USART_SR_RXNE (1 << 5)
#define USART_SR_TXE  (1 << 7)

#define POSITIVE 1
#define NEGATIVE 0

// PCLK1 is 16 MHz on reset. 16000000 / (16 * 9600) = 104.1667,
// so mantissa 104 and fraction 3/16.
#define USART3_BRR_9600 ((104 << 4) | 3)

volatile int g_mode = POSITIVE;


void gpio_init(void)
{
    // PB0 and PB7 to general purpose output, 01
    GPIOB_MODER &= ~((0x3u << (LED_HEARTBEAT * 2)) | (0x3u << (LED_BUTTON * 2)));
    GPIOB_MODER |=  ((0x1u << (LED_HEARTBEAT * 2)) | (0x1u << (LED_BUTTON * 2)));
    GPIOB_ODR   &= ~((1u << LED_HEARTBEAT) | (1u << LED_BUTTON));

    // PC13 to input, 00
    GPIOC_MODER &= ~(0x3u << (BUTTON_PIN * 2));
}


void usart3_init(void)
{
    // PD8 and PD9 to alternate function mode, 10
    GPIOD_MODER &= ~((0x3u << (8 * 2)) | (0x3u << (9 * 2)));
    GPIOD_MODER |=  ((0x2u << (8 * 2)) | (0x2u << (9 * 2)));

    // AF7 selects USART3. PD8 uses AFRH[3:0], PD9 uses AFRH[7:4].
    GPIOD_AFRH &= ~((0xFu << 0) | (0xFu << 4));
    GPIOD_AFRH |=  ((0x7u << 0) | (0x7u << 4));

    USART3_BRR = USART3_BRR_9600;

    // bit 13 USART enable, bit 3 transmitter enable, bit 2 receiver enable
    USART3_CR1 = (1 << 13) | (1 << 3) | (1 << 2);
}


void uart3_send(char c)
{
    while (!(USART3_SR & USART_SR_TXE));
    USART3_DR = (uint32_t) c;
}


char uart3_receive(void)
{
    while (!(USART3_SR & USART_SR_RXNE));
    return (char) (USART3_DR & 0xFF);
}


void LedTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        GPIOB_ODR ^= (1u << LED_HEARTBEAT);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void ButtonTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        int pressed = (GPIOC_IDR & (1u << BUTTON_PIN)) != 0;
        int on = (g_mode == POSITIVE) ? pressed : !pressed;

        if (on)
            GPIOB_ODR |=  (1u << LED_BUTTON);
        else
            GPIOB_ODR &= ~(1u << LED_BUTTON);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void UartTask(void *argument)
{
    (void)argument;
    char ch;

    for (;;)
    {
        if (USART3_SR & USART_SR_RXNE)
        {
            ch = uart3_receive();

            if (ch == 'p')
            {
                g_mode = POSITIVE;
                uart3_send('p');
            }
            else if (ch == 'n')
            {
                g_mode = NEGATIVE;
                uart3_send('n');
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}


int main(void)
{
    // GPIOB bit 1, GPIOC bit 2, GPIOD bit 3
    RCC_AHB1ENR |= (1 << 1) | (1 << 2) | (1 << 3);

    // USART3 bit 18 on APB1
    RCC_APB1ENR |= (1 << 18);

    gpio_init();
    usart3_init();

    xTaskCreate(LedTask,    "LED",    128, NULL, 2, NULL);
    xTaskCreate(ButtonTask, "BUTTON", 128, NULL, 1, NULL);
    xTaskCreate(UartTask,   "UART",   256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) { }
}