#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/vfs.h>

#define MAX_NUM_DRIVERS 2   /* realistically we will never have more than one FS */

/* store the callback functions here */
static fs_detect_cb driver_cbs[MAX_NUM_DRIVERS];
static int driver_count = 0;


void fs_register(fs_detect_cb probe)
{
    if (driver_count >= MAX_NUM_DRIVERS) {
        printk("fs_register: max number of FS drivers, hard coded limit\n");
        return;
    } else {
        driver_cbs[driver_count] = probe;
        driver_count++;
    }
}


struct superblock *fs_probe(block_dev *dev)
{
    for (int i = 0; i < driver_count; i++) {
        struct superblock *sb = driver_cbs[i](dev);    /* call each FS' probe */
        if (sb) {
            return sb;
        }
    }

    printk("fs_probe: none of the FS probe functions were successful\n");

    return NULL;
}