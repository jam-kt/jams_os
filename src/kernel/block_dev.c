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

block_dev *BLK_get_head(void) 
{
    return dev_list_head;
}