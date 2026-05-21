#include <stdint-gcc.h>

#include <userlib.h>

#define SYS_YIELD 0
#define SYS_EXIT 1
#define SYS_GETC 2
#define SYS_PUTC 3
#define SYS_WAIT 4
#define SYS_EXEC 5
#define SYS_FORK 6
#define SYS_CLONE 7
#define SYS_GETPID 8
#define SYS_GETTID 9
#define SYS_THREAD_EXIT 10
#define SYS_THREAD_JOIN 11
#define SYS_MUTEX_CREATE 12
#define SYS_MUTEX_LOCK 13
#define SYS_MUTEX_UNLOCK 14
#define SYS_MUTEX_DESTROY 15

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

static inline uint64_t syscall2(uint64_t num, uint64_t a1, uint64_t a2) 
{
    uint64_t ret;
    asm volatile (
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "movq %3, %%rsi\n"
        "int $128\n"
        "movq %%rax, %0"
        : "=r"(ret) 
        : "r"(num), "r"(a1), "r"(a2)
        : "rax", "rdi", "rsi", "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall4(uint64_t num, uint64_t a1, uint64_t a2,
                                uint64_t a3, uint64_t a4)
{
    uint64_t ret;
    register uint64_t r10 asm("r10") = a4;

    asm volatile (
        "movq %2, %%rax\n"
        "movq %3, %%rdi\n"
        "movq %4, %%rsi\n"
        "movq %5, %%rdx\n"
        "int $128\n"
        "movq %%rax, %0"
        : "=r"(ret), "+r"(r10)
        : "r"(num), "r"(a1), "r"(a2), "r"(a3)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
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

void yield(void)
{
    syscall0(SYS_YIELD);
}

void exit(int status) 
{
    syscall1(SYS_EXIT, (uint64_t)status);
    while (1) {
    }
}

uint64_t fork(void)
{
    return clone(CLONE_PROCESS, 0, 0, 0);
}

uint64_t clone(uint64_t mode, void *child_stack_top,
               void (*entry)(void *), void *arg)
{
    return syscall4(SYS_CLONE, mode, (uint64_t)child_stack_top,
                    (uint64_t)entry, (uint64_t)arg);
}

uint64_t getpid(void)
{
    return syscall0(SYS_GETPID);
}

uint64_t gettid(void)
{
    return syscall0(SYS_GETTID);
}

uint64_t thread_create(void *stack, uint64_t stack_size,
                       void (*entry)(void *), void *arg)
{
    uint8_t *stack_top;

    if (!stack || !stack_size || !entry) {
        return (uint64_t)-1;
    }

    stack_top = (uint8_t *)stack + stack_size;
    return clone(CLONE_THREAD, stack_top, entry, arg);
}

void thread_exit(int status)
{
    syscall1(SYS_THREAD_EXIT, (uint64_t)status);
    while (1) {
    }
}

uint64_t thread_join(uint64_t tid, int *status)
{
    return syscall2(SYS_THREAD_JOIN, tid, (uint64_t)status);
}

int mutex_init(mutex_t *mutex)
{
    uint64_t id;

    if (!mutex) {
        return -1;
    }

    id = syscall0(SYS_MUTEX_CREATE);
    if (!id) {
        mutex->id = 0;
        return -1;
    }

    mutex->id = id;
    return 0;
}

int mutex_lock(mutex_t *mutex)
{
    if (!mutex || !mutex->id) {
        return -1;
    }

    return (int)syscall1(SYS_MUTEX_LOCK, mutex->id);
}

int mutex_unlock(mutex_t *mutex)
{
    if (!mutex || !mutex->id) {
        return -1;
    }

    return (int)syscall1(SYS_MUTEX_UNLOCK, mutex->id);
}

int mutex_destroy(mutex_t *mutex)
{
    int ret;

    if (!mutex || !mutex->id) {
        return -1;
    }

    ret = (int)syscall1(SYS_MUTEX_DESTROY, mutex->id);
    if (ret == 0) {
        mutex->id = 0;
    }

    return ret;
}

uint64_t wait(int *status)
{
    return syscall1(SYS_WAIT, (uint64_t)status);
}

uint64_t exec(const char *filename)
{
    return syscall1(SYS_EXEC, (uint64_t)filename);
}
