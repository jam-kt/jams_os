# CPE454-kernel
CPE454 Kernel Project


CPE453-kernel/
├── .gitignore
├── Makefile
├── README.md
├── make-image.sh
├── src/
│   ├── arch/
│   │   └── x86_64/
│   │       ├── drivers/
│   │       │   ├── vga.c
│   │       │   └── ps2_kbd.c
│   │       ├── boot.asm
│   │       ├── grub.cfg
│   │       ├── linker.ld
│   │       ├── long_mode_init.asm
|   |       └── multiboot_header.asm
│   ├── include/
│   │   └── kernel/
│   │       ├── vga.h
│   │       └── ps2_kbd.h
|   └── kernel/
│       └── main.c
├── libc/
│   ├── arch/
│   │   └── x86_64/
│   ├── include/
│   │   ├── stdio.h
│   │   └── string.h
│   ├── string.c
│   └── stdio.c


If permission error occurs wheb trying to $make run then ensure the shell script 
is executable. $chmod +x make_image.sh

To debug:

gdb
set arch i386:x86-64:intel
symbol-file build/kernel-x86_64.bin
target remote localhost:1234


IRQ Line	Vector (hex)	Vector (dec)	Typical Device
0	        0x20	        32	            System timer (PIT)
1	        0x21	        33	            Keyboard controller
2	        0x22	        34	            Cascade for slave PIC (IRQs 8–15)
3	        0x23	        35	            COM2/COM4 serial port
4	        0x24	        36          	COM1/COM3 serial port
5	        0x25	        37	            LPT2 or (sound card)
6	        0x26	        38	            Floppy disk controller
7	        0x27	        39	            LPT1 parallel port
8	        0x28	        40  	        Real-Time Clock (RTC)
9	        0x29	        41	            ACPI or (spare for PCI)
10	        0x2A	        42	            PCI IRQ2 (or “reserved”)
11	        0x2B	        43	            PCI IRQ3 (or “reserved”)
12	        0x2C	        44  	        PS/2 Mouse
13	        0x2D	        45	            FPU / Coprocessor / Inter-Chip
14	        0x2E	        46	            Primary ATA channel (IDE0)
15	        0x2F	        47	            Secondary ATA channel (IDE1)