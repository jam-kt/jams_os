ENTRY(start)

SECTIONS {
    . = 1M;

    .boot :
    {
        /* ensure that the multiboot header is at the beginning */
        *(.multiboot_header)
    }

    .text :
    {
        *(.text)
    }

    .rodata :
    {
        *(.rodata)
    }

    .data :
    {
        *(.data)
    }

    .bss (NOLOAD) : 
    {
        __bss_start = .;     /* symbol: start of BSS */
        *(.bss)
        *(COMMON)
        __bss_end = .;       /* symbol: end of BSS */
    }

}