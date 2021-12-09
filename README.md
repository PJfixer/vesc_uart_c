# vesc_uart_c

C library to communicate with VESC(Vedder) opensoure esc.

Based on RollingGecko arduino C++ library.

Modified to run on STM32 with HAL.

I added methods to support DUAL-VESC CAN forwarding so i can get both vesc value through a single VESC Uart.

Also added method to set speed control (ERPM)  instead of current.
