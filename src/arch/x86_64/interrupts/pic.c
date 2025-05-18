#include <stddef.h>
#include <stdint-gcc.h>

#include <kernel/interrupts.h>
#include <stdio.h>

#include "pic.h"

/*******************************************************************************
 * ASM HELPERS
 ******************************************************************************/
/* inb and outb wrappers using intel syntax */
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ("inb  %0, %1"
                   : "=a"(ret)
                   : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile ("outb %1, %0"
                   :
                   : "a"(val), "Nd"(port));
}

/* invoke a small delay by writing to an unused port */
static inline void io_wait(void)
{
    outb(0x80, 0);
}


/*******************************************************************************
 * PIC API
 ******************************************************************************/
void ISR34_IRQ2_cascade(int vector, int err, void *arg);
static void PIC_remap(int offset1, int offset2);


void PIC_init()
{
    /* reinitialize the PIC, change vector mapping [0x00, 0x1F]->[0x20, 0x2F] */
    PIC_remap(PIC1_VECTOR, PIC2_VECTOR);

    /* register a no-op ISR for IRQ 2 (vector 34) (cascade notification) */
    register_interrupt(PIC1_VECTOR + 2, 0, TYPE_INTRGATE, ISR34_IRQ2_cascade, NULL);
}


/*
 * From OSDevWiki
 * - offset1: vector offset for master PIC vectors (offset1...offset1+7)
 * - offset2: vector offset for slave PIC vectors (offset2...offset2+7)
 */
static void PIC_remap(int offset1, int offset2)
{
    /* starts the initialization sequence (in cascade mode) */ 
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, offset1);       /* ICW2: Master PIC vector offset */ 
    io_wait();
    outb(PIC2_DATA, offset2);       /* ICW2: Slave PIC vector offset */ 
    io_wait();
    /* ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100) */
    outb(PIC1_DATA, 4);
    io_wait();
    /* ICW3: tell Slave PIC its cascade identity (0000 0010) */ 
    outb(PIC2_DATA, 2);
    io_wait();
    
    /* ICW4: have the PICs use 8086 mode (and not 8080 mode) */ 
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Unmask both PICs */ 
    outb(PIC1_DATA, 0);
    outb(PIC2_DATA, 0);
}

/* Setting mask disables the IRQ line */
void IRQ_set_mask(uint8_t irq_line) 
{
    uint16_t port;
    uint8_t value;

    if (irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }
    
    value = inb(port) | (1 << irq_line);
    outb(port, value);        
}

/* Clearing mask enables the IRQ line */
void IRQ_clear_mask(uint8_t irq_line) 
{
    uint16_t port;
    uint8_t value;

    if (irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }

    value = inb(port) & ~(1 << irq_line);
    outb(port, value);        
}

/* returns a boolean. 1 if the line is NOT enabled, 0 if it is */
int IRQ_get_mask(int irq_line)
{
    uint16_t port;
    uint8_t mask;

    if (irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }

    mask = inb(port);
    return (mask >> irq_line) & 1;
}

/* 
 * signal the PIC that the IRQ has been serviced. Checks for valid vector num.
 * Must call this function after ever IRQ (vectors 32-47)
 */
void IRQ_end_of_interrupt(uint8_t vector)
{
    if ((vector >= PIC2_VECTOR) && (vector < PIC2_VECTOR + 8)) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    
    if ((vector >= PIC1_VECTOR) && (vector < PIC1_VECTOR + 16)) {
        outb(PIC1_COMMAND, PIC_EOI);
    }
}

void ISR34_IRQ2_cascade(int vector, int err, void *arg)
{  
    IRQ_end_of_interrupt(vector);
}