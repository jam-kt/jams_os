#ifndef __MBR_H__
#define __MBR_H__

#include <stdint-gcc.h>
#include <stddef.h>

#include <kernel/block_dev.h>

#define MBR_SIGNATURE 0xAA55
#define MBR_BOOTABLE  0x80

typedef struct __attribute__ ((packed)) MBR_partition {
    uint8_t status;         /* bootable = 0x80, inactive = 0x0  */
    uint8_t chs_start[3];   /* ignore                           */
    uint8_t type;
    uint8_t chs_end[3];     /* ignore                           */
    uint32_t lba_start;
    uint32_t sector_count;
} MBR_partition;

struct __attribute__ ((packed)) MBR {
    uint8_t bootstrap[446];        /* 446 bytes of bootstrap code      */
    MBR_partition partitions[4];   /* partition entries                */
    uint16_t boot_sig;             /* signature should equal 0x55 and 0xAA */
};

typedef struct partition_dev {
    block_dev dev;          /* inherit from generic block device    */
    block_dev *parent;      /* underlying device (ATA)              */
    uint32_t lba_offset;    /* partition's start sector             */
} partition_dev;


void MBR_init(struct block_dev *dev);

#endif