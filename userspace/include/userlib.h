#ifndef __USERLIB_H__
#define __USERLIB_H__

#include <stdint-gcc.h>

void putc(char c);
void puts(const char *s);
void putnum(uint64_t n);
char getc(void);
void exit(int status);
uint64_t fork(void);
uint64_t wait(int *status);
uint64_t exec(const char *filename);

#endif
