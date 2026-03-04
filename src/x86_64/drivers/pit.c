#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <string.h>
#include <kernel/interrupts.h>
#include <kernel/kmalloc.h>
#include <kernel/syscall.h>
#include <kernel/memory.h>
#include <kernel/scheduler.h>
#include <kernel/multitask.h>

#define PIT_DIVISOR 11932   /* results in ~10ms atomic (100hz) */
#define MODE_PORT   0x43
#define CHAN0_PORT  0x40

void ISR32_pit(int vector, int err, void *rsp);


/* TODO: move inline ASM helpers to a common file for all kernel space */
static inline void outb(uint16_t port, uint8_t val) 
{
    asm volatile ("outb %1, %0" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) 
{
    uint8_t ret;
    asm volatile ("inb %0, %1" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* only call after multitasking has been initialized */
void pit_init()
{
    int enable = are_interrupts_enabled();
    CLI();

    /* set to use channel 0, mode 2 (rate divisor), 16-bit binary */
    outb(MODE_PORT, 0x34);

    /* set reload to divisor (hi, lo) */
    outb(CHAN0_PORT, PIT_DIVISOR & 0xFF);
    outb(CHAN0_PORT, (PIT_DIVISOR & 0xFF00) >> 8);

    register_interrupt(32, 0, TYPE_INTRGATE, 0, ISR32_pit, NULL);

    if (enable) {
        STI();
    }
}

void ISR32_pit(int vector, int err, void *rsp)
{
    /* calling reschedule is sufficient since our interrupt ASM will check 
     * if curr and next proc are different during every interrupt
     */
    PROC_reschedule();
}