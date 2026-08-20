#ifndef USART_H_
#define USART_H_

#ifndef F_CPU
#error "F_CPU must be defined to calculate USART3's baud rate register value"
#endif

#include <avr/io.h>
#include <stdbool.h>

// NOT a full USART HAL for the ATmega4809
// simply util functions to output on USART3 for debugging peripherals
// (aka things not supported by Microchip Studio's debugger)

#endif /* USART_H_ */