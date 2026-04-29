#include <stdint-gcc.h>

#include <userlib.h>

#define SYS_EXIT 1
#define SYS_GETC 2
#define SYS_PUTC 3
#define SYS_WAIT 4
#define SYS_EXEC 5
#define SYS_FORK 6

static inline uint64_t syscall0(uint64_t num) 
{
    uint64_t ret;
    asm volatile (
        "movq %1, %%rax\n"
        "int $128\n"
        "movq %%rax, %0"
        : "=r"(ret) 
        : "r"(num) 
        : "rax", "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall1(uint64_t num, uint64_t a1) 
{
    uint64_t ret;
    asm volatile (
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "int $128\n"
        "movq %%rax, %0"
        : "=r"(ret) 
        : "r"(num), "r"(a1)
        : "rax", "rdi", "rcx", "r11", "memory"
    );
    return ret;
}

void putc(char c) 
{
    syscall1(SYS_PUTC, (uint64_t)c);
}

void puts(const char *s)
{
    while (*s) {
        putc(*s++);
    }
}

void putnum(uint64_t n)
{
    char buf[21];
    int i = 0;

    if (n == 0) {
        putc('0');
        return;
    }

    while (n > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }

    while (i > 0) {
        putc(buf[--i]);
    }
}

char getc(void) 
{
    return (char)syscall0(SYS_GETC);
}

void exit(int status) 
{
    syscall1(SYS_EXIT, (uint64_t)status);
    while (1) {
    }
}

uint64_t fork(void)
{
    return syscall0(SYS_FORK);
}

uint64_t wait(int *status)
{
    return syscall1(SYS_WAIT, (uint64_t)status);
}

uint64_t exec(const char *filename)
{
    return syscall1(SYS_EXEC, (uint64_t)filename);
}
