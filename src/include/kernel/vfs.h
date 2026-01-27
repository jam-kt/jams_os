#ifndef __VFS_H__
#define __VFS_H__

#include <stdint-gcc.h>
#include <stddef.h>

#include <kernel/block_dev.h>

/* inode mode macros */
#define S_IFDIR 0x4000
#define S_IFREG 0x8000

/* avoiding typedef-ing these for now since the names are too common */
struct superblock;  
struct inode;
struct file;

/* Callback for directory iteration: name, inode pointer, user-data */
typedef int (*readdir_cb)(const char *name, struct inode *inode, void *p);

struct superblock {
    const char *name;
    struct inode *root_inode;
    
    struct inode *(*read_inode)(struct superblock *sb, uint32_t inode_num);
    
    void *priv_data;            /* ptr to the FS driver's private struct */
};

struct inode {
    uint32_t ino_num;
    uint32_t size;
    uint32_t mode;              /* 0x4000 dir, 0x8000 file */
    struct superblock *sb;      /* ptr back to SB */

    int (*readdir)(struct inode *inode, readdir_cb cb, void *p);
    struct file *(*open)(struct inode *inode);
    
    void *priv_data;            /* ptr to the FS driver's private inode */
};

struct file {
    struct inode *inode;
    uint64_t offset;

    int (*read)(struct file *f, void *buf, int len);
    int (*lseek)(struct file *f, int offset);
    int (*close)(struct file *f);
};

/* interface for filesystem detection function */
typedef struct superblock *(*fs_detect_cb)(block_dev *dev);

void fs_register(fs_detect_cb probe);
struct superblock *fs_probe(block_dev *dev);

#endif