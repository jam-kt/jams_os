#ifndef __BLOCK_DEV_H__
#define __BLOCK_DEV_H__

#include <stddef.h>
#include <stdint-gcc.h>

enum block_dev_type { MASS_STORAGE, PARTITION };

typedef struct block_dev {
   uint64_t tot_length;
   int (*read_block)(struct block_dev *dev, uint64_t blk_num, void *dst);
   uint32_t blk_size;
   enum block_dev_type type;
   const char *name;
   uint8_t fs_type;
   struct block_dev *next;
} block_dev;

int blk_register(block_dev *dev);
block_dev *blk_get_head(void);
block_dev *blk_find_partiton_fstype(uint8_t fs_type);

#endif