#ifndef __IDT_TSS_H__
#define __IDT_TSS_H__

#include <stddef.h>
#include <stdint-gcc.h>


/* ---------------------------- IDT ---------------------------- */
void idt_init();
void add_idt_entry(uint8_t vector, uint8_t IST, uint8_t type, uint8_t DPL);

#define STUB_SIZE 9   /* each ASM ISR stub is 9 bytes */

/* see the bottom of boot.asm for GDT layout */
#define KERNEL_CS   0x08    /* kernel code segment GDT offset */
#define KERNEL_DS   0x10    /* kernel data segment */
#define TSS_DESC_CS 0x28    /* TSS GDT offset      */

/* 
 * IDT entry (Interrupt Gate Descriptor)
 * A 16-byte entry in the Interrupt Descriptor Table
 */
struct idt_entry {
    uint16_t offset1;   /* forms 64-bit function ptr to the ISR               */
    uint16_t select;    /* GDT selector (should be kernel's code segment)     */

    uint16_t IST:3;     /* interrupt stack table index (0 to stay, else 1-7 ) */
    uint16_t IGN1:5;    /* reserved         */
    uint16_t type:4;    /* type             */
    uint16_t zero:1;    /* always zero      */
    uint16_t DPL:2;     /* protection level */
    uint16_t pres:1;    /* present          */

    uint16_t offset2;
    uint32_t offset3;
    uint32_t IGN2;      /* reserved         */
} __attribute__((packed));

/* load this struct using lidt instruction */
struct idt_descriptor {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));


/* ---------------------------- TSS ---------------------------- */
void tss_init();

#define TSS_STACK_SIZE  4096
#define TSS_TYPE        0x9     /* magic number, means TSS is available */

/* TSS structs */
struct tss_table {
    uint32_t IGN0;      /* reserved, ignore */
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t IGN1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t IGN2;
    uint16_t IGN3;
    uint16_t io_map_base;
} __attribute__((packed));

struct tss_descriptor {
    uint16_t limit1;    /* segment limit [0:15] */
    uint16_t base1;     /* base address [0:15]  */
    uint8_t  base2;     /* base address [16:23] */

    uint8_t type:4;     /* type                 */
    uint8_t zero:1;     /* always zero          */
    uint8_t DPL:2;      /* protection level     */
    uint8_t pres:1;     /* present              */

    uint8_t limit2:4;   /* limit [16:19]        */
    uint8_t AVL:1;      /* bit available to OS  */
    uint8_t IGN:2;      /* ignore               */
    uint8_t gran:1;     /* granularity          */

    uint8_t  base3;     /* base address [24:31] */
    uint32_t base4;     /* base address [32:63] */
    uint32_t reserved;
} __attribute__((packed));

#endif