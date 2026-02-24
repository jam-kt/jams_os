#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>
#include <kernel/bbuf.h>
#include <kernel/serial_out.h>


static void ISR36_IRQ4_serial(int vector, int err, void *arg);


#define PORT                0x3f8          // COM1
#define IIR_NO_PENDING      (1 << 0)
#define IIR_MASK            0x0E
#define IIR_THR_EMPTY       0x02
#define LSR_THR_EMPTY       (1 << 5)

#define SER_BUFFER_SIZE     2048

static int ser_active = 0;
static char ser_buf[SER_BUFFER_SIZE];
static struct bbuf_st ser_st;


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

int serial_init() {
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x01);    // Set divisor to 1 (lo byte) 115200 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // enable hardware fifo
    outb(PORT + 4, 0x03);    // MCR = 0b00000011 (DTR=1, RTS=1)

    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(PORT + 4, 0x0F);

    outb(PORT + 1, 0x02);   /* enable 'TX holding reg empty' interrupt */

    /* bounded buffer, ISR consumes to send bytes over serial */
    bbuf_init(&ser_st, ser_buf, SER_BUFFER_SIZE);

    register_interrupt(36, 0, TYPE_INTRGATE, ISR36_IRQ4_serial, NULL);

    return 0;
}

static int transmit_empty(void)
{
    return inb(PORT + 5) & LSR_THR_EMPTY;
}

static void transmit(void)
{
    int enable_ints = 0;
    if (are_interrupts_enabled()) {
        enable_ints = 1;
        CLI();
    }

    /* check and clear busy status */
    if (ser_active && transmit_empty()) {
        ser_active = 0;
    }

    /* attempt to send if not busy */
    if (!ser_active) {
        char c;
        if (!bbuf_try_consume(&ser_st, &c)) {
            ser_active = 1;
            outb(PORT, c);
        }
    }

    if (enable_ints) {
        STI();
    }
}

void serial_display_char(char c)
{
    bbuf_try_add(&ser_st, c);
    transmit();
}

void serial_display_string(const char *s)
{
    for (; *s; ++s) {
        serial_display_char(*s); 
    }
}

/* register as an interrupt gate so interrupts are disabled by default */
static void ISR36_IRQ4_serial(int vector, int err, void *arg)
{
    (void)vector;
    (void)err;
    (void)arg;

    uint8_t iir = inb(PORT + 2);
    if (iir & IIR_NO_PENDING) {
        return;
    }

    /* only handle THR-empty interrupt cause */
    if ((iir & IIR_MASK) == IIR_THR_EMPTY) {
        ser_active = 0;
        transmit();
    }
}
