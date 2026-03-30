/* io.h -- UART I/O routines for PL/SW compiler on COR24 */

#ifndef IO_H
#define IO_H

/* ---- UART hardware ---- */

#define UART_DATA   0xFF0100
#define UART_STATUS 0xFF0101

void uart_putchar(int ch) {
    while (*(char *)UART_STATUS & 0x80) {}
    *(char *)UART_DATA = ch;
}

int uart_getchar(void) {
    while (!(*(char *)UART_STATUS & 0x01)) {}
    return *(char *)UART_DATA;
}

void uart_puts(char *s) {
    while (*s) {
        uart_putchar(*s);
        s = s + 1;
    }
    uart_putchar(10);
}

void uart_putstr(char *s) {
    while (*s) {
        uart_putchar(*s);
        s = s + 1;
    }
}

/* Read a line with backspace/DEL support. Returns length. */
int uart_getline(char *buf, int len) {
    int pos = 0;
    int maxpos = len - 1;

    while (1) {
        int ch = uart_getchar();

        if (ch == 10 || ch == 13) {
            uart_putchar(10);
            buf[pos] = 0;
            return pos;
        }

        if (ch == 8 || ch == 127) {
            if (pos > 0) {
                pos--;
                uart_putchar(8);
                uart_putchar(32);
                uart_putchar(8);
            }
        } else if (ch >= 32 && pos < maxpos) {
            buf[pos] = ch;
            pos++;
            uart_putchar(ch);
        }
    }
}

/* ---- Numeric I/O ---- */

void print_int(int n) {
    if (n < 0) {
        uart_putchar(45);  /* '-' */
        n = 0 - n;
    }
    if (n == 0) {
        uart_putchar(48);
        return;
    }
    char buf[8];
    int i = 0;
    while (n > 0) {
        buf[i] = 48 + n % 10;
        n = n / 10;
        i++;
    }
    while (i > 0) {
        i--;
        uart_putchar(buf[i]);
    }
}

/* Parse decimal integer from string. Returns value, sets *end to
   index past last digit consumed. Returns 0 with *end == start
   if no digits found. */
int parse_int(char *s, int start, int *end) {
    int i = start;
    int neg = 0;
    int val = 0;
    int got = 0;

    if (s[i] == 45) {  /* '-' */
        neg = 1;
        i++;
    }

    while (s[i] >= 48 && s[i] <= 57) {
        val = val * 10 + (s[i] - 48);
        i++;
        got = 1;
    }

    if (got == 0) {
        *end = start;
        return 0;
    }

    *end = i;
    if (neg) {
        return 0 - val;
    }
    return val;
}

#endif
