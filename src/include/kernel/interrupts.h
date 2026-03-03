#ifndef __INTERRUPTS_H__
#define __INTERRUPTS_H__

#include <stddef.h>
#include <stdint-gcc.h>


#define NUM_ISRS 256

/* ISR type. Interrupts run with interrupts disabled. Traps have enabled. */
#define TYPE_TRAPGATE 0xF
#define TYPE_INTRGATE 0xE

/* callback function */
typedef void (*isr_t) (int vector, int err, void* arg);

/* an entry into the C ISR table */
struct isr_c_entry {
    void *arg;
    isr_t handler;  /* a function pointer to the ISR specific for a vector */
};

void interrupts_init();
void CLI();
void STI();
int are_interrupts_enabled();
void register_interrupt(int vector, uint8_t IST, uint8_t type, uint8_t DPL, isr_t handler, void *arg);

#endif