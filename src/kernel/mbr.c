#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/kmalloc.h>
#include <kernel/block_dev.h>
#include <kernel/mbr.h>


static int partition_read_block(block_dev *dev, uint64_t blk_num, void *dst);


void MBR_init(struct block_dev *dev)
{
    if (!dev) {
        printk("MBR_init: block device is NULL\n");
        return;
    }

    struct MBR *mbr = kmalloc(sizeof(struct MBR));
    if (!mbr) {
        printk("MBR_init: failed to allocate mbr struct\n");
        return;
    }

    /* read block 0 of the device to find MBR */
    if (dev->read_block(dev, 0, mbr) != 0) {
        printk("MBR_init: could not read block 0, check device?\n");
        kfree(mbr);
        return;
    }

    /* validate boot signature */
    if (mbr->boot_sig != MBR_SIGNATURE) {
        printk("MBR invalid signature: expect 0xAA55 found %x\n", mbr->boot_sig);
        kfree(mbr);
        return;
    }

    printk("Found a valid MBR\n");

    for (int i = 0; i < 4; i++) {
        MBR_partition *part = &mbr->partitions[i];

        if (part->type == 0) {  /* check for empty partitons */
            continue;
        }

        printk("found partition %d\n", i);
        printk("status: %x %s\n", part->status,
            (part->status == MBR_BOOTABLE) ? "(bootable)" : "(inactive)");
        printk("type: %x\n", part->type);
        printk("LBA start addr: %x\n", part->lba_start);
        printk("sectors: %d\n\n", part->sector_count);

        /* create a new block device for the valid partition */
        partition_dev *pdev = kmalloc(sizeof(partition_dev));
        if (!pdev) {
            printk("MBR_init: couldn't kmalloc for a new block dev\n");
            continue;
        }

        pdev->dev.read_block = partition_read_block;
        pdev->dev.blk_size = dev->blk_size;
        pdev->dev.tot_length = part->sector_count;
        pdev->dev.type = PARTITION;
        pdev->dev.name = "PARTITION";
        pdev->dev.fs_type = part->type;     /* use for finding this partion */


        pdev->parent = dev;
        pdev->lba_offset = part->lba_start;

        blk_register((block_dev *)pdev);
    }

    kfree(mbr);
}

/* allows us to use relative to partition sectors for FS reading */
static int partition_read_block(block_dev *dev, uint64_t blk_num, void *dst)
{
    partition_dev *part = (partition_dev *)dev;
    /* add the partition offset to the requested block number */
    uint64_t real_blk = blk_num + part->lba_offset;
    return part->parent->read_block(part->parent, real_blk, dst);
}