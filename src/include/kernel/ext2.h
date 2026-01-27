#ifndef __EXT2_FS_H__
#define __EXT2_FS_H__

#include <stdint-gcc.h>

#define EXT2_SIGNATURE  0xEF53
#define EXT2_ROOT_INODE 2
#define EXT2_TYPE       0x83

void ext2_init();

typedef struct __attribute__((packed)) ext2_superblock {
    uint32_t inodes_count;      /* self explanatory, important */
    uint32_t blocks_count;      /* self explanatory, important */
    uint32_t r_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;    /* log2 (block size) - 10 */
    uint32_t log_frag_size;
    uint32_t blocks_per_group;  /* self explanatory, important */
    uint32_t frags_per_group;
    uint32_t inodes_per_group;  /* self explanatory, important */
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;             /* ext2 signature (must be 0xEF53) */
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;         /* major version number */
    uint16_t def_resuid;
    uint16_t def_resgid;
    /* extended fields (if major version >= 1)*/
    uint32_t non_resv_inode;
    uint32_t inodes_size;       /* size of inodes in bytes */

    uint8_t  padding[932];      /* total size must be 1024 bytes */ 
} ext2_superblock;

/* num groups = num blocks / blocks per group */
typedef struct __attribute__((packed)) ext2_group {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;       /* starting block addr of inode table */
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint8_t padding[14];
} ext2_group;

/*
 * group = (inode - 1) / (#inodes per group)
 * index = (inode - 1) % (#inodes per group)
 * block = (index * inode size) / block size
 * Inode numbers begin at 1, 0 means empty
 */
typedef struct __attribute__((packed)) ext2_inode {
    uint16_t mode;          /* type and permissions */
    uint16_t uid;
    uint32_t size;          /* lower 32 bits of size in bytes */
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;   
    uint32_t blocks;        /* count of ext2 disk sectors */
    uint32_t flags;
    uint32_t os;
    uint32_t block[15];     /* 0-11 direct, 12 single, 13 double, 14 triple */
    uint32_t generation;
    uint32_t acl;
    uint32_t faddr;         /* upper 32 bits of file size if version >= 1 */
    uint8_t  osd2[16];
} ext2_inode;

typedef struct __attribute__((packed)) ext2_directory {
    uint32_t inode;
    uint16_t rec_len;      
    uint8_t  name_len;      /* LSB of name length */
    uint8_t  file_type;     /* MSB of name length OR type if feature in SB */
    char     name[];
} ext2_directory;

#endif