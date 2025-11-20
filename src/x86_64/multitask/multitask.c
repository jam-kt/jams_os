#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>
#include <kernel/syscall.h>
#include <kernel/scheduler.h>
#include <kernel/multitask.h>


void syscall_yield(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                    uint64_t a5, uint64_t a6);


#define DEFAULT_STACK_BYTES (8 * 1024 * 1024)      /* binary 8MB */
#define x86_64_ALIGNMENT 16


static scheduler sched = round_robin;
static proc curr_proc = NULL;
static proc next_proc = NULL;


void multitask_init()
{
    register_syscall(SYS_YIELD_NUM, syscall_yield);
}

void syscall_yield(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                    uint64_t a5, uint64_t a6)
{
    printk("We made it into the yield syscall\n");

}

/* the public facing syscall */
void yield(void) 
{
    register long nr asm("rax") = SYS_YIELD_NUM;
    asm volatile("int 0x80" : "+a"(nr) :: "memory");
}