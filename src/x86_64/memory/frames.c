#include <stdint-gcc.h>

// #include <kernel/memory>
#include <stdio.h>

#include "mboot.h"
#include "frames.h"


static void parse_mmap_tag(struct mmap_tag *tag);
static void add_usable_region(uint64_t addr, uint64_t size, int entry);
static void parse_elf_tag(struct elf_tag *tag);
static void remove_region(uint64_t start, uint64_t end);


/* list of usable regions, does not cause a reference into the region (fast) */
static struct mem_region usable_regions[MAX_REGIONS];

/* list of freed frames, each frame header is stored within the frame (slow) */
static struct pf_header *free_frames = NULL;


void parse_mboot_tags(void *mboot_header)
{
    uint32_t total_size = 0;
    uint8_t *tag_addr = NULL;
    uint8_t *end_addr = NULL;

    /* read fixed header */ 
    struct mboot_fixed *fixed_tag = (struct mboot_fixed *)mboot_header;
    total_size = fixed_tag->total_size;
    end_addr = (uint8_t *)mboot_header + total_size;

    /* point to next variable sized tag */
    tag_addr = (uint8_t *)mboot_header + sizeof(struct mboot_fixed);

    /* walk tags list */
    while (tag_addr < end_addr) {
        struct mboot_tag *tag = (struct mboot_tag *)tag_addr;

        switch (tag->type) {
        case TAG_TYPE_MMAP:
            parse_mmap_tag((struct mmap_tag *)tag);
            break;

        case TAG_TYPE_ELF:
            parse_elf_tag((struct elf_tag *)tag);
            break;

        default:
            break;
        }

        /* move to next 8 byte aligned tag */
        tag_addr += (tag->tag_size + 7) & (~7);
    }
}

static void parse_mmap_tag(struct mmap_tag *tag)
{
    struct mmap_entry *entries = (struct mmap_entry *)(tag + sizeof(struct mmap_tag));
    int num = (tag->tag_size - sizeof(struct mmap_tag)) / tag->entry_size;

    /* walk entries, add usable ones to the usable regions list */
    for (int i = 0; i < num; i++) {
        if (entries[i].type == REGION_OS_USE) {
            add_usable_region(entries[i].addr, entries[i].size, i);
        }
    }
}

/* add a region to the usable regions list */
static void add_usable_region(uint64_t addr, uint64_t size, int entry)
{
    /* align region start and end to page(s) boundaries */
    uint64_t start = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); /* wasteful ? */
    uint64_t end = (addr + size) & ~(PAGE_SIZE - 1);

    if (end <= start) {
        return;
    }

    if (entry >= MAX_REGIONS) {
        printk("Page frame allocator: no more regions!!\n");
        return;
    }

    /* add the region to the region list */
    usable_regions[entry].addr = start;
    usable_regions[entry].size = end - start;
    usable_regions[entry].valid = 1;
}

static void parse_elf_tag(struct elf_tag *tag)
{
    struct sect_head *headers = (struct sect_head *)(tag + sizeof(struct elf_tag));

    /* walk headers and mark kernel text/data regions as occupied (unusable) */
    for (int i = 0; i < tag->num_sect; i++) {
        uint64_t start = headers[i].addr & ~(PAGE_SIZE - 1);
        uint64_t end = (headers[i].addr + headers[i].size) & ~(PAGE_SIZE - 1);
        remove_region(start, end);
    }
}

/* 
 * remove the region from addresses start to end from the usable region list 
 * Note that both start and end should be aligned to a frame
 */
static void remove_region(uint64_t start, uint64_t end)
{
    for (int i = 0; i < MAX_REGIONS; i++) {
        if (!usable_regions[i].valid) {
            continue;
        }

        uint64_t reg_start = usable_regions[i].addr;
        uint64_t reg_end = reg_start + usable_regions[i].size;

        /* check for regions NOT overlapping */
        if (end <= reg_start || start >= reg_end) {
            continue;
        }

        /* check for full overlapping of regions */
        if (start <= reg_start && end >= reg_end) {
            usable_regions[i].valid = 0;    /* just mark region as invalid */
            continue;
        }

        /* bad region is smaller than current region, resize usable portion */
        if (start <= reg_start && end < reg_end) { /* front part of region */
            usable_regions[i].addr = end;
            usable_regions[i].size = reg_end - end;
            continue;
        }

        if (start > reg_start && end >= reg_end) { /* back part of region */
            usable_regions[i].size = start - reg_start;
            continue;
        }

        /* bad region is within the usable region, split region into 2 chunks */
        uint64_t left_start = reg_start;
        uint64_t left_size = start - reg_start;
        uint64_t right_start = end;
        uint64_t right_size = reg_end - end;

        /* current entry to left chunk */
        usable_regions[i].addr = left_start;
        usable_regions[i].size = left_size;

        /* find any free slot for the right chunk */
        for (int j = 0; j < MAX_REGIONS; j++) {
            if (!usable_regions[j].valid) {
                add_usable_region(right_start, right_size, j);
                break;
            }
        }
    }
}

/* assumes that regions are always in multiple of page frame sizes */
void *MMU_pf_alloc()
{
    /* try to allocate from previously freed frames */
    if (free_frames) {
        struct pf_header *node = free_frames;
        free_frames = node->next;
        return (void *)node;
    }

    /* otherwise allocate from available regions */
    for (int i = 0; i < MAX_REGIONS; i++) {
        if (!usable_regions[i].valid) {
            continue;
        }

        /* if the region has 1 frame left, take it and mark as invalid */
        if (usable_regions[i].size == PAGE_SIZE) {
            usable_regions[i].valid = 0;
            return (void *)usable_regions[i].addr;
        }

        /* the region still has more than 1 frame, take 1 and resize region */
        uint64_t frame = usable_regions[i].addr;
        usable_regions[i].addr += PAGE_SIZE;
        usable_regions[i].size -= PAGE_SIZE;

        return (void *)frame;
    }

    /* no more frames!! */
    printk("Page frame allocator: no more page frames!!\n");
    return NULL;
}

void MMU_pf_free(void *pf)
{
    if (pf == NULL) {
        return;
    }

    struct pf_header *header = (struct pf_header *)pf;
    header->next = free_frames;
    free_frames = header;
}
