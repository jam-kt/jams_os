#include <stdarg.h>
#include <stdbool.h>
#include <stdint-gcc.h>
#include <stddef.h>

#include <stdio.h>
#include <kernel/vga.h>
#include <kernel/serial_out.h>

static int putchar(int c)
{
    vga_display_char((char)c);
    serial_display_char((char)c);
    return c;
}

static void putstr(const char *str)
{
    vga_display_str(str);
    serial_display_string(str);
}

static void print_unsigned(uint64_t num, unsigned base) 
{
    char buf[32];
    const char *digits = "0123456789ABCDEF";
    int i = 0;

    if (num == 0) {
        buf[i] = '0';
        i++;
    } else {
        while (num > 0) {
            buf[i] = digits[num % base];
            i++;
            num /= base;             /* "left shift" the number in its base */
        }
    }

    while (i > 0) {
        i--;
        putchar(buf[i]);
    }
}

static void print_signed(int64_t num) 
{
    if (num < 0) {
        putchar('-');
        print_unsigned((uint64_t)(-num), 10);
    } else {
        print_unsigned((uint64_t)num, 10);
    }
}

int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

int printk(const char *fmt, ...) 
{
    va_list ap;
    va_start(ap, fmt);
    const char *p;
    int written = 0;

    for (p = fmt; *p; p++) {
        /* print char until we reach % */
        if (*p != '%') {
            putchar(*p);
            written++;
            continue;
        }

        /* go to char after '%' and identify what format is specified */
        p++;
        bool lmod_h = false;
        bool lmod_l = false;
        bool lmod_q = false;
        if (*p == 'h') {
            lmod_h = true;
            p++;
        } else if (*p == 'l') {
            lmod_l = true;
            p++;
        } else if (*p == 'q') {
            lmod_q = true;
            p++;
        }

        switch (*p) {
        case '%':
            putchar('%');
            ++written;
            break;

        case 'c':
            char c = (char)va_arg(ap, int);
            putchar(c);
            written++;
            break;

        case 's':
            const char *s = va_arg(ap, const char *);
            if (s == NULL) {
                s = "(null)";
            }
            putstr(s);
            while (*s++) {
                written++;
            }
            break;

        case 'p':
            uintptr_t ptr = (uintptr_t)va_arg(ap, void *);
            putstr("0x");
            written += 2;
            print_unsigned(ptr, 16);
            break;

        case 'd':
            if (lmod_q) {
                print_signed(va_arg(ap, long long));
            } else if (lmod_l) {
                print_signed(va_arg(ap, long));
            } else if (lmod_h) {
                print_signed((short)va_arg(ap, int));
            } else {
                print_signed(va_arg(ap, int));
            }
            break;

        case 'u':
            uint64_t u;
            if (lmod_q) {
                u = va_arg(ap, unsigned long long);
            } else if (lmod_l) {
                u = va_arg(ap, unsigned long);
            } else if (lmod_h) {
                u = (unsigned short)va_arg(ap, unsigned);
            } else {
                u = va_arg(ap, unsigned);
            }
            print_unsigned(u, 10);
            break;

        case 'x':
            putstr("0x");
            written += 2;
            uint64_t ux;
            if (lmod_q) {
                ux = va_arg(ap, unsigned long long);
            } else if (lmod_l) {
                ux = va_arg(ap, unsigned long);
            } else if (lmod_h) {
                ux = (unsigned short)va_arg(ap, unsigned);
            } else {
                ux = va_arg(ap, unsigned);
            }
            print_unsigned(ux, 16);
            break;

        default:
            putstr("\nUnknown format\n");
            return written;
        }
    }

    va_end(ap);
    return written;
}