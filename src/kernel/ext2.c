#include <stdint-gcc.h>
#include <stddef.h>

#include <string.h>
#include <stdio.h>
#include <kernel/kmalloc.h>
#include <kernel/memory.h>
#include <kernel/block_dev.h>
#include <kernel/vfs.h>
#include <kernel/ext2.h>

/* internal to ext2 driver */
typedef struct ext2_driver {
    block_dev *dev;
    ext2_superblock sb;
    uint32_t block_size;
    /* block number where the block group descriptor table starts */
    uint32_t block_start;
} ext2_driver;

/* forward declaration for the "overridden" VFS methods */
static struct inode *ext2_read_inode(struct superblock *sb, uint32_t inode_num);
static int ext2_readdir(struct inode *inode, readdir_cb cb, void *p);
struct superblock *ext2_probe(block_dev *dev);

void ext2_init()
{
    fs_register(ext2_probe);
}


struct superblock *ext2_probe(block_dev *dev) {
    /* read 1024 bytes starting at offset 1024 (sectors 2, 3) */
    uint8_t *buf = kmalloc(1024);
    dev->read_block(dev, 2, buf);
    dev->read_block(dev, 3, buf + 512);

    ext2_superblock *sb = (ext2_superblock *)buf;

    if (sb->magic != EXT2_SIGNATURE) {
        printk("EXT2: this FS is not of type ext2, got %x\n", sb->magic);
        kfree(buf);
        return NULL;
    }

    /* valid ext2 */
    printk("EXT2: valid ext2 signature found on %s\n", dev->name);
    
    struct superblock *vsb = kmalloc(sizeof(struct superblock));
    ext2_driver *drv = kmalloc(sizeof(ext2_driver));
    
    memcpy(&drv->sb, sb, sizeof(ext2_superblock));
    drv->dev = dev;
    drv->block_size = 1024 << sb->log_block_size;
    
    /* if block_size == 1024 BGDT is in block 2 else block 1. */
    drv->block_start = (drv->block_size == 1024) ? 2 : 1;

    vsb->name = "ext2";
    vsb->priv_data = drv;
    vsb->read_inode = ext2_read_inode;
    
    /* read Root Inode */
    vsb->root_inode = ext2_read_inode(vsb, EXT2_ROOT_INODE);
    
    kfree(buf);
    return vsb;
}


static int ext2_read_block(ext2_driver *driver, uint32_t block_num, void *buf)
{
    /* ext2 block size and ata sector size do not necessarily match */
    uint32_t sect_per_blk = driver->block_size / driver->dev->blk_size;
    uint64_t start_sect = (uint64_t)block_num * sect_per_blk;

    char *cbuf = (char *)buf;
    for (uint32_t i = 0; i < sect_per_blk; i++) {
        uint32_t offset = i * driver->dev->blk_size;
        driver->dev->read_block(driver->dev, start_sect + i, cbuf + offset);
    }

    return 0;
}

/* pass in inode num and returns a corresponding inode structure */
static struct inode *ext2_read_inode(struct superblock *sb, uint32_t inode_num)
{
    ext2_driver *drv = (ext2_driver *)sb->priv_data;
    
    /* use 0 indexing for sanity */
    uint32_t idx = inode_num - 1;
    uint32_t group = idx / drv->sb.inodes_per_group;
    uint32_t group_index = idx % drv->sb.inodes_per_group;
    
    /* read the block containing the group descriptor */
    void *bg_buf = kmalloc(drv->block_size);
    uint32_t descriptors_per_block = drv->block_size / sizeof(ext2_group);
    uint32_t bg_block_offset = group / descriptors_per_block;
    uint32_t bg_index_in_block = group % descriptors_per_block;
    
    ext2_read_block(drv, drv->block_start + bg_block_offset, bg_buf);
    
    ext2_group *bg = (ext2_group *)bg_buf + bg_index_in_block;
    uint32_t inode_table_block = bg->inode_table;
    
    /* find block inside the inode table */
    uint32_t inode_size = (drv->sb.rev_level >= 1) ? drv->sb.inodes_size : 128;
    uint32_t inodes_per_block = drv->block_size / inode_size;
    uint32_t table_block_offset = group_index / inodes_per_block;
    uint32_t index_in_block = group_index % inodes_per_block;
    
    /* read block containing the inode */
    void *inode_block_buf = kmalloc(drv->block_size);
    ext2_read_block(drv, inode_table_block + table_block_offset, inode_block_buf);
    
    ext2_inode *raw_node = (ext2_inode *)((uintptr_t)inode_block_buf + 
        (index_in_block * inode_size));
    
    /* make generic VFS inode */
    struct inode *vnode = kmalloc(sizeof(struct inode));
    vnode->ino_num = inode_num;
    vnode->size = raw_node->size;
    vnode->mode = raw_node->mode;
    vnode->sb = sb;
    vnode->readdir = ext2_readdir;
    
    /* save raw inode data */
    vnode->priv_data = kmalloc(inode_size);
    memcpy(vnode->priv_data, raw_node, inode_size);
    
    kfree(bg_buf);
    kfree(inode_block_buf);
    return vnode;
}


/* given a directory inode interpret the directory content and find child */
static int ext2_readdir(struct inode *inode, readdir_cb cb, void *p) {
    if ((inode->mode & S_IFDIR) == 0) {
        return -1;
    } 

    ext2_inode *raw_node = (ext2_inode *)inode->priv_data;
    struct superblock *sb = inode->sb;
    ext2_driver *drv = (ext2_driver *)sb->priv_data;
    
    void *block_buf = kmalloc(drv->block_size);
    
    /* NOTE: only checks direct blocks for now */
    for (int i = 0; i < 12; i++) {
        if (raw_node->block[i] == 0) {
            break;
        }

        ext2_read_block(drv, raw_node->block[i], block_buf);

        uint32_t offset = 0;
        while (offset < drv->block_size) {
            /* walk through block and look for valid ext2 directory entries */
            ext2_directory *dentry = 
                (ext2_directory *)((uintptr_t)block_buf + offset);

            if (dentry->rec_len < 8 || (dentry->rec_len % 4) != 0) {
                break;
            }
            
            if (dentry->inode != 0) {
                /* make null terminated string */
                char name[256];
                memcpy(name, dentry->name, dentry->name_len);
                name[dentry->name_len] = '\0';
                
                struct inode *child = sb->read_inode(sb, dentry->inode);
                if (child) {
                    /* callback allows us to recurse if needed */
                    cb(name, child, p); 
                    kfree(child->priv_data);
                    kfree(child);
                }   
            }
            
            offset += dentry->rec_len;
        }
    }

    kfree(block_buf);
    return 0;
}