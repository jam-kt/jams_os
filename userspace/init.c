#include <stdint-gcc.h>

#define SYS_KEXIT 1
#define SYS_GETC  2
#define SYS_PUTC  3

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

char getc() 
{
    return (char)syscall0(SYS_GETC);
}

void kexit() 
{
    syscall0(SYS_KEXIT);
}

static void trigger_fault_isr()
{
    asm volatile("int $13");
}

static void trigger_fault_page()
{
    volatile uint64_t x = *(volatile uint64_t *)0x1000;
    (void)x;
}

int main(void) 
{
    putc('H');
    putc('e');
    putc('l');
    putc('l');
    putc('o');
    putc('\n');

    while (1) {
        char c = getc();

        if (c == '1') {
            trigger_fault_isr();
        } else if (c == '2') {
            trigger_fault_page();
        }

        putc(c);
    }
    
    return 0;
}

/* entry point expected by linker */
void _start(void) 
{
    main();
}
