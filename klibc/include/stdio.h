#ifndef __STDIO_H__
#define __STDIO_H__

#include <stdarg.h>
#include <stdbool.h>
#include <stdint-gcc.h>
#include <stddef.h>

int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
