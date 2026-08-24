# STM32 FreeRTOS UART LED Control

A small FreeRTOS project on the STM32F439ZI Nucleo-144. Three tasks run at once: one blinks an LED, one drives a second LED from the USER button, and one listens on the serial port. Typing `p` or `n` over UART flips whether the button LED turns on when the button is pressed or when it is released.

The same project is here twice. `hal_version` uses ST's HAL driver library. `baremetal_version` does the exact same thing by writing straight to the registers.

## Why Two Versions

I built the HAL version first to get it working. Then I rewrote it against the reference manual to understand what the HAL was actually doing underneath. Setting up USART3 by hand meant working out the baud rate divisor myself, figuring out which alternate function number maps to USART3, and finding the right bits in the RCC registers to turn the peripheral clocks on.

Both versions behave identically on the board.

## Hardware

- NUCLEO-F439ZI
- Onboard ST-LINK for flashing and debugging over SWD
- USB serial at 9600 baud, 8N1

## Pin Mapping

| Function | Pin |
| --- | --- |
| Heartbeat LED | PB0 |
| Button indicator LED | PB7 |
| USER button (B1) | PC13 |
| USART3 TX | PD8 |
| USART3 RX | PD9 |

## Tasks

| Task | Priority | What it does |
| --- | --- | --- |
| LED | 2 | Toggles PB0 every 500 ms so you can tell the scheduler is running |
| BUTTON | 1 | Reads PC13 every 10 ms and drives PB7 based on the current mode |
| UART | 1 | Reads incoming characters and switches the mode |

Mode is shared between the button and UART tasks through a `volatile int`. A 32-bit aligned integer is written atomically on a Cortex-M4, so no mutex is needed here.

## Serial Commands

| Character | Result |
| --- | --- |
| `p` | Button LED turns on when the button is pressed |
| `n` | Button LED turns on when the button is released |

Both are echoed back so you know the board got them.

## Clock Setup

Both versions run on the internal 16 MHz HSI with no PLL, so SYSCLK and PCLK1 are both 16 MHz.

That matters for the bare-metal version. The baud rate register is set by hand to `(104 << 4) | 3`, which comes from 16000000 divided by (16 times 9600). If the clock tree changes, that number has to change with it. The HAL version recalculates it on its own, which is one of the tradeoffs between the two.

## Build

Import either folder into STM32CubeIDE as an existing project, build, and flash over the onboard ST-LINK. Open a serial terminal at 9600 baud on the ST-LINK virtual COM port to send commands.

The bare-metal version never calls `HAL_Init()`, so FreeRTOS gets its tick straight from the port layer. `configCPU_CLOCK_HZ` in `FreeRTOSConfig.h` has to be `16000000` or the delays will be off.
