#include <stdint-gcc.h>
#include <stddef.h>

#include <string.h>
#include <stdio.h>
#include <kernel/kmalloc.h>
#include <kernel/memory.h>
#include <kernel/block_dev.h>
#include <kernel/vfs.h>
#include <kernel/ext2.h>

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

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
static struct file *ext2_open(struct inode *inode);
static int ext2_file_read(struct file *f, void *buf, int len);
static int ext2_file_lseek(struct file *f, int offset);
static int ext2_file_close(struct file *f);
static uint32_t ext2_get_block_number(struct inode *inode, uint32_t logic_blk);

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
    vnode->open = ext2_open;
    
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

    struct superblock *sb = inode->sb;
    ext2_driver *drv = (ext2_driver *)sb->priv_data;
    
    void *block_buf = kmalloc(drv->block_size);
    
    uint32_t bytes_left = inode->size;
    uint32_t total_blocks = (bytes_left + drv->block_size - 1) / drv->block_size;

    for (uint32_t blk = 0; blk < total_blocks; blk++) {
        uint32_t phys_block = ext2_get_block_number(inode, blk);
        uint32_t block_bytes = MIN(bytes_left, drv->block_size);

        if (phys_block == 0) {
            bytes_left -= block_bytes;
            continue;
        }

        ext2_read_block(drv, phys_block, block_buf);

        uint32_t offset = 0;
        while (offset < block_bytes) {
            /* walk through block and look for valid ext2 directory entries */
            ext2_directory *dentry = 
                (ext2_directory *)((uintptr_t)block_buf + offset);

            if (dentry->rec_len < 8 || (dentry->rec_len % 4) != 0) {
                break;
            }
            
            if (offset + dentry->rec_len > block_bytes) {
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

        bytes_left -= block_bytes;
    }

    kfree(block_buf);
    return 0;
}


/* 
 * translates file relative logical block number into a volume relative physical 
 * block number.
 */
static uint32_t ext2_get_block_number(struct inode *inode, uint32_t logic_blk) {
    ext2_inode *raw_node = (ext2_inode *)inode->priv_data;
    struct superblock *sb = inode->sb;
    ext2_driver *drv = (ext2_driver *)sb->priv_data;
    
    /* ptr per block = block_size / 4 . (block indices are 4 bytes) */
    uint32_t p = drv->block_size / 4; 

    /* direct blocks (0-11) */
    if (logic_blk < 12) {
        return raw_node->block[logic_blk];
    }
    logic_blk -= 12;

    /* singly indirect block (12) */
    if (logic_blk < p) {
        if (raw_node->block[12] == 0) {
            return 0;
        }

        uint32_t *block_buf = kmalloc(drv->block_size);
        ext2_read_block(drv, raw_node->block[12], block_buf);
        
        uint32_t phys_block = block_buf[logic_blk];
        kfree(block_buf);
        return phys_block;
    }

    logic_blk -= p;

    /* doubly indirect block (13) */
    if (logic_blk < (p * p)) {
        if (raw_node->block[13] == 0) {
            return 0; // Sparse
        }

        uint32_t idx_1 = logic_blk / p;
        uint32_t idx_2 = logic_blk % p;

        uint32_t *block_buf = kmalloc(drv->block_size);
        
        /* read the first level indirect block */
        ext2_read_block(drv, raw_node->block[13], block_buf);
        uint32_t indirect_block = block_buf[idx_1];
        
        if (indirect_block == 0) {
            kfree(block_buf);
            return 0; 
        }

        /* read the second level indirect block */
        ext2_read_block(drv, indirect_block, block_buf);
        uint32_t phys_block = block_buf[idx_2];
        
        kfree(block_buf);
        return phys_block;
    }
    
    /* triply indirect block (14) */
    if (raw_node->block[14] == 0) {
        return 0;
    }

    uint32_t idx_1 = logic_blk / (p * p);
    uint32_t rem = logic_blk % (p * p);
    uint32_t idx_2 = rem / p;
    uint32_t idx_3 = rem % p;

    uint32_t *block_buf = kmalloc(drv->block_size);

    /* read the first level indirect block */
    ext2_read_block(drv, raw_node->block[14], block_buf);
    uint32_t indirect_block_1 = block_buf[idx_1];
    if (indirect_block_1 == 0) {
        kfree(block_buf);
        return 0;
    }

    /* read the second level indirect block */
    ext2_read_block(drv, indirect_block_1, block_buf);
    uint32_t indirect_block_2 = block_buf[idx_2];
    if (indirect_block_2 == 0) {
        kfree(block_buf);
        return 0;
    }

    /* read the third level indirect block */
    ext2_read_block(drv, indirect_block_2, block_buf);
    uint32_t phys_block = block_buf[idx_3];

    kfree(block_buf);
    return phys_block;
}

static struct file *ext2_open(struct inode *inode) {
    struct file *f = kmalloc(sizeof(struct file));
    if (!f) {
        return NULL;
    }

    f->inode = inode;
    f->offset = 0;
    f->read = ext2_file_read;
    f->lseek = ext2_file_lseek;
    f->close = ext2_file_close;

    return f;
}

static int ext2_file_read(struct file *f, void *buf, int len) {
    if (!f || !f->inode) {
        return -1;
    }

    struct inode *inode = f->inode;
    ext2_driver *drv = (ext2_driver *)inode->sb->priv_data;
    uint8_t *out_buf = (uint8_t *)buf;
    
    /* check bounds */
    if (f->offset >= inode->size) {
        return 0; 
    }

    if (f->offset + len > inode->size) {
        len = inode->size - f->offset;
    }

    int bytes_read = 0;
    void *temp_block = kmalloc(drv->block_size);

    while (len > 0) {
        /* determine block of the current offset */
        uint32_t logical_block = f->offset / drv->block_size;
        uint32_t block_offset = f->offset % drv->block_size;
        
        /* read whats available or requested, whichever is smaller */
        uint32_t to_read = MIN(len, drv->block_size - block_offset);

        uint32_t phys_block = ext2_get_block_number(inode, logical_block);

        if (phys_block == 0) {
            /* nothing in 0th block */
            memset(out_buf, 0, to_read);
        } else {
            ext2_read_block(drv, phys_block, temp_block);
            memcpy(out_buf, temp_block + block_offset, to_read);
        }

        out_buf += to_read;
        f->offset += to_read;
        len -= to_read;
        bytes_read += to_read;
    }

    kfree(temp_block);
    return bytes_read;
}


static int ext2_file_lseek(struct file *f, int offset) {
    /* check bound then update offset */
    if (offset > f->inode->size) {
        return -1;
    }

    f->offset = offset;

    return 0;
}


static int ext2_file_close(struct file *f) {
    if (f) {
        kfree(f); 
    }

    return 0;
}