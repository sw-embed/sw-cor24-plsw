#include "io.h"

#define LINE_MAX 128

int main() {
    uart_puts("PL/SW Compiler v0.1");
    uart_puts("COR24 target");
    uart_puts("");

    char line[LINE_MAX];

    while (1) {
        uart_putstr("> ");
        int len = uart_getline(line, LINE_MAX);

        if (len == 0) {
            continue;
        }

        /* echo back what was typed */
        uart_putstr("got ");
        print_int(len);
        uart_putstr(" chars: ");
        uart_puts(line);

        /* test parse_int on the input */
        int end = 0;
        int val = parse_int(line, 0, &end);
        if (end > 0) {
            uart_putstr("parsed int: ");
            print_int(val);
            uart_putchar(10);
        }
    }

    return 0;
}
