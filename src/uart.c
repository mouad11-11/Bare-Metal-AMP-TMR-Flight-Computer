#include "uart.h"
#include "memory_map.h"

#define UARTDR      (*(volatile uint32_t *)(PL011_UART0_BASE + 0x00))
#define UARTFR      (*(volatile uint32_t *)(PL011_UART0_BASE + 0x18))
#define UARTIBRD    (*(volatile uint32_t *)(PL011_UART0_BASE + 0x24))
#define UARTFBRD    (*(volatile uint32_t *)(PL011_UART0_BASE + 0x28))
#define UARTLCR_H   (*(volatile uint32_t *)(PL011_UART0_BASE + 0x2C))
#define UARTCR      (*(volatile uint32_t *)(PL011_UART0_BASE + 0x30))

#define UARTFR_TXFF (1U << 5) /* Transmit FIFO full */
#define UARTFR_BUSY (1U << 3) /* UART busy */

void uart_init(void) {
    /* Disable UART */
    UARTCR = 0x0;
    
    /* Set baud rate (vexpress standard: 38400 or 115200 with 24MHz clock) */
    UARTIBRD = 0x27; /* 39 */
    UARTFBRD = 0x04; /* 4 */
    
    /* 8 bits, no parity, 1 stop bit, FIFO enabled */
    UARTLCR_H = (0x3 << 5) | (1 << 4);
    
    /* Enable UART, transmit and receive */
    UARTCR = (1 << 0) | (1 << 8) | (1 << 9);
}

void uart_putc(char c) {
    if (c == '\n') {
        uart_putc('\r');
    }
    /* Wait until FIFO is not full */
    while (UARTFR & UARTFR_TXFF) {
        /* spin */
    }
    UARTDR = (uint32_t)c;
}

void uart_puts(const char *str) {
    if (!str) return;
    while (*str) {
        uart_putc(*str++);
    }
}

void uart_put_uint(uint32_t val) {
    char buf[12];
    int i = 0;
    if (val == 0) {
        uart_putc('0');
        return;
    }
    while (val > 0) {
        buf[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

void uart_put_int(int32_t val) {
    if (val < 0) {
        uart_putc('-');
        uart_put_uint(0U - (uint32_t)val);
    } else {
        uart_put_uint((uint32_t)val);
    }
}

void uart_put_hex(uint32_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        uart_putc(hex_chars[(val >> i) & 0xF]);
    }
}

void uart_printf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        p++;

        /* Parse optional field width */
        int width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        switch (*p) {
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
                uart_putc(c);
                break;
            }
            case 's': {
                const char *s = __builtin_va_arg(args, const char *);
                if (!s) s = "(null)";
                int len = 0;
                while (s[len]) len++;
                while (len < width) {
                    uart_putc(' ');
                    width--;
                }
                uart_puts(s);
                break;
            }
            case 'd': {
                int32_t d = __builtin_va_arg(args, int32_t);
                char buf[16];
                int i = 0;
                uint32_t val = (d < 0) ? (0U - (uint32_t)d) : (uint32_t)d;
                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    while (val > 0) {
                        buf[i++] = (char)('0' + (val % 10));
                        val /= 10;
                    }
                }
                if (d < 0) buf[i++] = '-';
                while (i < width) {
                    uart_putc(' ');
                    width--;
                }
                while (i > 0) {
                    uart_putc(buf[--i]);
                }
                break;
            }
            case 'u': {
                uint32_t u = __builtin_va_arg(args, uint32_t);
                char buf[16];
                int i = 0;
                if (u == 0) {
                    buf[i++] = '0';
                } else {
                    while (u > 0) {
                        buf[i++] = (char)('0' + (u % 10));
                        u /= 10;
                    }
                }
                while (i < width) {
                    uart_putc(' ');
                    width--;
                }
                while (i > 0) {
                    uart_putc(buf[--i]);
                }
                break;
            }
            case 'x':
            case 'X': {
                uint32_t x = __builtin_va_arg(args, uint32_t);
                uart_put_hex(x);
                break;
            }
            case '%': {
                uart_putc('%');
                break;
            }
            default: {
                uart_putc('%');
                if (*p != '\0') uart_putc(*p);
                break;
            }
        }
    }

    __builtin_va_end(args);
}
