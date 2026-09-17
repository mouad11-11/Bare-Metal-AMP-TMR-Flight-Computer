#ifndef UART_H
#define UART_H

#include "types.h"

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *str);
void uart_put_int(int32_t val);
void uart_put_uint(uint32_t val);
void uart_put_hex(uint32_t val);
void uart_printf(const char *fmt, ...);

#endif /* UART_H */
