#ifndef __ELF64_H__
#define __ELF64_H__

#include <stdint-gcc.h>
#include <kernel/vfs.h>

struct ELF_common_head {
    uint8_t magic[4];
    uint8_t bitsize;
    uint8_t endian;
    uint8_t version;
    uint8_t abi;
    uint64_t padding;
    uint16_t exe_type;
    uint16_t isa;
    uint32_t elf_version;
} __attribute__((packed));

struct ELF64_head {
    struct ELF_common_head common;
    uint64_t prog_entry_pos;
    uint64_t prog_table_pos;
    uint64_t sec_table_pos;
    uint32_t flags;
    uint16_t hdr_size;
    uint16_t prog_ent_size;
    uint16_t prog_ent_num;
    uint16_t sec_ent_size;
    uint16_t sec_ent_num;
    uint16_t sec_name_idx;
} __attribute__((packed));

struct ELF64_prog_head {
    uint32_t type;
    uint32_t flags;
    uint64_t file_offset;
    uint64_t load_addr;
    uint64_t undefined;
    uint64_t file_size;
    uint64_t mem_size;
    uint64_t alignment;
} __attribute__((packed));

void elf_load(struct inode *root, const char *filename);

#endif