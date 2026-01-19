#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <string.h>
#include <kernel/kmalloc.h>
#include <kernel/interrupts.h>
#include <kernel/multitask.h>
#include <kernel/block_dev.h>
#include <kernel/ata.h>


/* ATA IO port offsets */
#define REG_DATA        0x0
#define REG_ERROR       0x1   /* read */
#define REG_FEATURES    0x1   /* write */
#define REG_SEC_COUNT0  0x2
#define REG_LBA0        0x3
#define REG_LBA1        0x4
#define REG_LBA2        0x5
#define REG_HDDEVSEL    0x6
#define REG_STATUS      0x7   /* read */
#define REG_COMMAND     0x7   /* write */

/* ATA commands */
#define CMD_IDENTIFY        0xEC
#define CMD_READ_PIO_EXT    0x24  /* LBA48 read */
#define CMD_CACHE_FLUSH     0xE7

/* status bitmasks */
#define STATUS_BSY      0x80
#define STATUS_DRQ      0x08
#define STATUS_ERR      0x01
#define STATUS_DF       0x20

/* status register bits */
#define ATA_SR_BSY     0x80    /* busy                  */
#define ATA_SR_DRDY    0x40    /* drive ready           */
#define ATA_SR_DRQ     0x08    /* data request ready    */
#define ATA_SR_ERR     0x01    /* error                 */


/* represents an ATA IO operation request */
typedef struct ata_request {
    uint64_t blk_num;           /* block to read                        */
    void *buffer;               /* destination                          */
    proc_queue wait_queue;      /* queue to block the calling thread    */
    int done;                   /* flag set by ISR when complete        */
    struct ata_request *next;
} ata_request;

typedef struct ata_block_dev {
    block_dev dev;              /* inherit from the generic block device    */
    uint16_t  ata_base;         /* IO base                                  */
    uint16_t  ata_master;       /* ATA controller base addr                 */
    uint8_t   slave;            /* 0 = master, 1 = slave                    */
    uint8_t   irq;
    ata_request *req_head;      /* two pointers to form a queue             */
    ata_request *req_tail;
} ata_block_dev;


static int ata_read_block(block_dev *dev, uint64_t blk_num, void *dst);
static void ISR_ata_handler(int vector, int err, void *arg);
static void ata_start_request(ata_block_dev *ata, ata_request *req);


/* --------------------------------------------------------------------------
 * PORT IO HELPERS
 * -------------------------------------------------------------------------- */
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

static inline uint16_t inw(uint16_t port) 
{
    uint16_t ret;
    asm volatile ("inw %0, %1" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* reads cnt words (16-bit) from port to addr */
static inline void insw(uint16_t port, void *addr, uint32_t cnt) {
    asm volatile ("rep insw" : "+D"(addr), "+c"(cnt) : "d"(port) : "memory");
}

static void ata_delay(uint16_t base)
{
    /* ~400ns delay */
    for(int i = 0; i < 15; i++) {
        inb(base + REG_STATUS);
    }
}


/* --------------------------------------------------------------------------
 * DRIVER LOGIC
 * -------------------------------------------------------------------------- */
 block_dev *ata_probe(uint16_t base, uint16_t master,  uint8_t slave, const char *name, uint8_t irq)
{
    if (inb(base + REG_STATUS) == 0xFF) {       /* check for floating */
        printk("ATA: floating bus\n");
        return NULL;
    }

    /* IDENTIFY command */
    outb(base + REG_SEC_COUNT0, 0);
    outb(base + REG_LBA0, 0);
    outb(base + REG_LBA1, 0);
    outb(base + REG_LBA2, 0);
    outb(base + REG_COMMAND, CMD_IDENTIFY);
    
    if (inb(base + REG_STATUS) == 0) {
        printk("ATA: device does not exist\n");
        return NULL;
    }

    while (inb(base + REG_STATUS) & STATUS_BSY);

    /* check for ATAPI/SATA signatures in LBA mid/high registers */
    uint8_t mid = inb(base + REG_LBA1);
    uint8_t hi  = inb(base + REG_LBA2);
    
    if (mid != 0 || hi != 0) {
        /* indicates the drive is ATAPI or SATA */
        printk("ATA: found non PATA device\n");
        return NULL;
    }

    /* wait for DRQ or ERR */
    while (1) {
        uint8_t status = inb(base + REG_STATUS);
        if (status & STATUS_ERR) {
            printk("ATA: encountered error on IDENTIFY\n");
            return NULL; 
        }

        if (status & STATUS_DRQ) {
            break;
        }
    }

    /* read identity data */
    uint16_t identity[256];
    insw(base + REG_DATA, identity, 256);

    /* confirm LBA48 support */
    if (!(identity[83] & (1 << 10))) {
        printk("ATA: the device: %s does not support LBA48\n", name);
        return NULL; 
    }

    /* identity[100] through identity[103] holds total 48-bit sectors */
    uint64_t total_sectors = *((uint64_t *)&identity[100]);

    /* init struct */
    ata_block_dev *ata = kmalloc(sizeof(ata_block_dev));
    if (!ata) {
        return NULL;
    }

    memset(ata, 0, sizeof(ata_block_dev));

    ata->ata_base = base;
    ata->ata_master = master;
    ata->slave = slave;
    ata->irq = irq;

    ata->dev.read_block = ata_read_block;
    ata->dev.blk_size = 512;
    ata->dev.tot_length = total_sectors;
    ata->dev.type = MASS_STORAGE;
    ata->dev.name = name;

    /* register */
    register_interrupt(irq, 0, TYPE_INTRGATE, ISR_ata_handler, ata);
    blk_register((block_dev *)ata);

    return (block_dev *)ata;
}


static void ata_start_request(ata_block_dev *ata, ata_request *req)
{
    uint16_t base = ata->ata_base;
    uint64_t lba = req->blk_num;
    
    /* wait for BSY to clear and DRQ to clear before sending new command */
    while (inb(base + REG_STATUS) & (STATUS_BSY | STATUS_DRQ));

    /* select drive and enable LBA mode */
    outb(base + REG_HDDEVSEL, 0x40 | ata->slave << 4);
    ata_delay(base);

    /* setup LBA48 regs */
    outb(base + REG_SEC_COUNT0, 0);
    outb(base + REG_SEC_COUNT0, 1);
    outb(base + REG_LBA0, (uint8_t)((lba >> 24) & 0xFF));
    outb(base + REG_LBA0, (uint8_t)((lba)       & 0xFF));
    outb(base + REG_LBA1, (uint8_t)((lba >> 32) & 0xFF));
    outb(base + REG_LBA1, (uint8_t)((lba >> 8)  & 0xFF));
    outb(base + REG_LBA2, (uint8_t)((lba >> 40) & 0xFF));
    outb(base + REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));

    /* read sectors EXT command */
    outb(base + REG_COMMAND, CMD_READ_PIO_EXT);
}


static int ata_read_block(block_dev *dev, uint64_t blk_num, void *dst)
{
    ata_block_dev *ata = (ata_block_dev *)dev;
    
    ata_request *req = kmalloc(sizeof(ata_request));
    if (!req) {
        return -1;
    }

    req->blk_num = blk_num;
    req->buffer = dst;
    req->done = 0;
    req->next = NULL;
    PROC_init_queue(&req->wait_queue);

    /* add to shared ATA block device request queue */
    CLI();
    
    if (ata->req_head == NULL) {
        ata->req_head = req;
        ata->req_tail = req;
        /* queue was empty, start immediately */
        ata_start_request(ata, req);
    } else {
        ata->req_tail->next = req;
        ata->req_tail = req;
    }

    /* block until ISR completes the request */
    wait_event_interruptable(req->wait_queue, req->done == 1);
    kfree(req);
    return 0;
}


static void ISR_ata_handler(int vector, int err, void *arg)
{
    ata_block_dev *ata = (ata_block_dev *)arg;
    uint8_t status = inb(ata->ata_base + REG_STATUS);
    if (status == 0) {
        return;
    }

    if (!(status & STATUS_DRQ)) {   /* check if data is ready */
        printk("returning early\n");
        return;
    }

    ata_request *req = ata->req_head;
    if (!req) {
        printk("ATA IRQ: no read request present\n");
        return;
    }

    /* buffer existence sanity check and then read data */
    if (!req->buffer) {
        printk("no ATA buffer\n");
        return;
    } else {
        insw(ata->ata_base + REG_DATA, req->buffer, 256);
    }
    
    /* request complete */
    req->done = 1;
    ata->req_head = req->next;
    PROC_unblock_all(&req->wait_queue);

    /* advance ATA driver queue */
    if (ata->req_head == NULL) {
        ata->req_tail = NULL;
    } else {
        /* start next request */
        ata_start_request(ata, ata->req_head);
    }
}