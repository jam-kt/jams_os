#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>
#include <kernel/bbuf.h>
#include <kernel/ps2_kbd.h>
#include <kernel/multitask.h>


static void ISR33_keyboard_irq(int vector, int err, void *arg);


/* I/O ports */
#define PS2_DATA      0x60
#define PS2_STATUS    0x64
#define PS2_CMD       0x64

/* Status flags */
#define PS2_STAT_OBF  0x01      /* output‑buffer full  (controller -> CPU) */
#define PS2_STAT_IBF  0x02      /* input‑buffer  full  (CPU -> controller) */

/* Keyboard replies */
#define KBD_ACK       0xFA
#define KBD_RESEND    0xFE

#define KBD_BUFFER_SIZE 16
#define KBD_ISR_NUM     33

static char kbd_buf[KBD_BUFFER_SIZE];
static struct bbuf_st kbd_st;

static proc_queue kbd_wait_queue;

/* Scancode set 1 map */
static const char map[0x3A] = {
  0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
  '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
  'a','s','d','f','g','h','j','k','l',';','\'','`',   0,
  '\\','z','x','c','v','b','n','m',',','.','/', 0, 0, 0,
  ' '
};


/* inb and outb wrappers using intel syntax */
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

/* reads a byte from the data port after wait-polling */
static uint8_t poll_read(void)
{
    while (!(inb(PS2_STATUS) & PS2_STAT_OBF));
    return inb(PS2_DATA);
}

/* write the data byte to the given port after wait-polling */
static void poll_write(uint16_t port, uint8_t data)
{
    while (inb(PS2_STATUS) & PS2_STAT_IBF);
    outb(port, data);
}

void ps2_init(void) 
{
    /* disable both ports */
    poll_write(PS2_CMD, 0xAD);
    poll_write(PS2_CMD, 0xA7);

    /* read controller configuration byte */
    poll_write(PS2_CMD, 0x20);
    uint8_t cfg = poll_read();

    /* enable IRQ1, enable clock1, disable IRQ2, disable clock2 */
    cfg |= 1;           /* IRQ1     */
    cfg |= (1 << 4);    /* CLK1     */
    cfg &= ~(1 << 1);   /* no IRQ2  */
    cfg &= ~(1 << 5);   /* no CLK2  */
    cfg |= (1 << 6);    /* enable set 2 -> set 1 translation (cleaner map) */

    /* write config back */
    poll_write(PS2_CMD, 0x60);
    poll_write(PS2_DATA, cfg);

    /* enable port 1 */
    poll_write(PS2_CMD, 0xAE);
}

/*  initialize keyboard to scancode */
void keyboard_init(void) 
{
    /* reset the keyboard */
    poll_write(PS2_DATA, 0xFF);

    int res = poll_read();
    if (res != KBD_ACK) {
        printk("kbd: no ACK after reset. Got %x\n", res);
        return;
    }

    /* check for self test value*/
    if (poll_read() != 0xAA) {
        printk("kbd: self-test failed\n");
        return;
    }

    /* flush any pending bytes */
    while (inb(PS2_STATUS) & PS2_STAT_OBF) {
        (void)inb(PS2_DATA);
    }

    /* set scancode set 2 (optional but common on PCs) */
    poll_write(PS2_DATA, 0xF0);
    poll_write(PS2_DATA, 0x02);
    res = poll_read();
    if (res != KBD_ACK) {
        printk("kbd: set scancode 2, no ACK. Got %x\n", res);
        return;
    }

    /* enable scanning */
    poll_write(PS2_DATA, 0xF4);
    res = poll_read();
    if (res != KBD_ACK) {
        printk("kbd: enable scanning no ACK. Got %x\n", res);
    }

    bbuf_init(&kbd_st, kbd_buf, KBD_BUFFER_SIZE);

    PROC_init_queue(&kbd_wait_queue);

    /* register keyboard interrupt */
    register_interrupt(KBD_ISR_NUM, 0, TYPE_INTRGATE, ISR33_keyboard_irq, NULL);
}

/* return ASCII character or -1 for unsupproted/no scancodes */
int keyboard_getchar(void) 
{
    char c;

    wait_event_interruptable(kbd_wait_queue, (bbuf_try_consume(&kbd_st, &c) == 0));

    return (int)c;
}

/* keyboard producer ISR. ACK to the PIC is handled by common C isr */
static void ISR33_keyboard_irq(int vector, int err, void *arg)
{
    uint8_t sc = inb(PS2_DATA);
    if ((sc == 0xF0) || (sc == 0xE0)) {     /* skip multibyte scancodes */
        return;
    }
    
    if (sc < 0x3A) {                        /* sanity check scancode    */
        bbuf_try_add(&kbd_st, map[sc]);     /* add to kbd buffer        */
        PROC_unblock_head(&kbd_wait_queue);
    }
}