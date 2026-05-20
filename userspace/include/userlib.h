#ifndef __USERLIB_H__
#define __USERLIB_H__

#include <stdint-gcc.h>

#define CLONE_PROCESS 0
#define CLONE_THREAD  1

void putc(char c);
void puts(const char *s);
void putnum(uint64_t n);
char getc(void);
void exit(int status);
uint64_t fork(void);
uint64_t clone(uint64_t mode, void *child_stack_top,
               void (*entry)(void *), void *arg);
uint64_t getpid(void);
uint64_t gettid(void);
uint64_t thread_create(void *stack, uint64_t stack_size,
                       void (*entry)(void *), void *arg);
void thread_exit(int status);
uint64_t thread_join(uint64_t tid, int *status);
uint64_t wait(int *status);
uint64_t exec(const char *filename);

#endif
