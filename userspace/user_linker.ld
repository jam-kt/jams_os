ENTRY(_start)

SECTIONS
{
    /* load at user space defined in vpages.h (slot 16) */
    . = 0x100000000000;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}