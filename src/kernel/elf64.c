#include <stdint-gcc.h>
#include <stddef.h>

#include <string.h>
#include <stdio.h>
#include <kernel/kmalloc.h>
#include <kernel/memory.h>
#include <kernel/block_dev.h>
#include <kernel/vfs.h>
#include <kernel/ext2.h>
#include <kernel/elf64.h>


struct ELFCommonHeader {
    uint8_t magic[4];
    uint8_t bitsize;
    uint8_t endian;
    uint8_t version;
    uint8_t abi;
    uint64_t padding;
    uint16_t exe_type;
    uint16_t isa;
    uint32_t elf_version;
};

struct ELF64Header {
    struct ELFCommonHeader common;
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
};
