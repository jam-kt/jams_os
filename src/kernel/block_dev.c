#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>
#include <kernel/bbuf.h>
#include <kernel/multitask.h>
#include <kernel/block_dev.h>


/* linked list of registered block devices */
static block_dev *dev_list_head = NULL;


int blk_register(block_dev *dev)
{
    if (!dev) {
        printk("blk_register: passed a NULL device\n");
        return -1;
    }

    /* insert at the head of the linked list */
    dev->next = dev_list_head;
    dev_list_head = dev;

    printk("Registered block device: %s (size: %lu blocks)\n", 
        dev->name ? dev->name : "Unknown device", dev->tot_length);

    return 0;
}

block_dev *blk_get_head(void) 
{
    return dev_list_head;
}

/* returns the first block device that matches the given fs_type */
block_dev *blk_find_partiton_fstype(uint8_t fs_type) 
{
    block_dev *curr = blk_get_head();
    
    while (curr) {
        if ((curr->type == PARTITION) && (curr->fs_type == fs_type)) {
            return curr;
        }
        curr = curr->next;
    }

    printk("blk_find_partiton_type: couldn't find a partition with type: %x\n", fs_type);
    return NULL;
}