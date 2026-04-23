# x86-64 Custom Kernel

An operating system kernel written from scratch in C and x86-64 assembly. Designed to move beyond high-level OS theory, this project implements a complete boot-to-userspace pipeline. The system boots via GRUB, establishes virtual memory management, schedules threads, parses an ext2 filesystem from an ATA-backed disk image, and securely executes ELF binaries within a fully isolated Ring 3 address space.

## Demo

<img width="630" height="350" alt="kerneldemo_small" src="https://github.com/user-attachments/assets/056451a3-17dd-4a53-b4bf-a372459d79dc" />

This demo shows me building the kernel image, and then booting it in QEMU with serial output to my terminal.
See [Building and Running](#building-and-running) for the full process!

## Table of Contents

- [Why I Built This](#why-i-built-this)
- [What The Kernel Does](#what-the-kernel-does)
- [Current Highlights](#current-highlights)
- [Memory Architecture](#memory-architecture)
- [Project Layout](#project-layout)
- [Building and Running](#building-and-running)
- [Debugging](#debugging)
- [Further Reading](#further-reading)

## Why I Built This

I built this project to get hands-on experience with systems programming at the
lowest levels. I wanted a project that forced me to understand how a machine
boots, how privileged code interacts with hardware, how memory translation is
established, and how a kernel begins to grow into a real operating system.

This kernel became my personal lab for learning:

- x86-64 boot flow and long mode setup
- interrupt handling, faults, and privilege boundaries
- physical and virtual memory management
- cooperative/preemptive scheduling foundations
- block devices, partition parsing, and ext2 filesystem traversal
- ELF loading and launching a simple user process

## What The Kernel Does

At a high level, the kernel currently:

- Boots via GRUB using a custom x86-64 kernel image
- Initializes interrupts, IDT/TSS state, PIC handling, and timer support
- Parses the multiboot memory map to build a physical frame allocator
- Sets up paging and virtual memory structures for kernel and user space
- Provides a basic heap allocator (`kmalloc`/`kfree`)
- Supports kernel threads and user threads with a round-robin scheduler
- Creates user processes with their own page-table root and hardware protection ring
- Exposes a system call interface (`int 0x80`) for user programs
- Talks to ATA storage via an interrupt-driven driver and registers block devices
- Parses an MBR partition table and locates an ext2 partition
- Mounts and reads an ext2 filesystem through a modular VFS layer
- Loads and launches a user-space ELF program from disk
- Verifies filesystem integrity via recursive directory traversal and MD5 hashing
- Supports serial output and keyboard input for simple interaction

## Current Highlights

These are the pieces that demonstrate the project's maturity:

- **Boot and architecture setup:** GRUB boot path, long mode initialization, custom linker scripts, GDT/IDT, and dynamic TSS management.
- **Interrupts and faults:** Interrupt registration, PIC setup, exception handling, and dedicated fault paths. 
- **Memory management:** Multiboot memory map parsing, 4-level page-table walking, and kernel heap allocation. Virtual pages utilize **demand paging**, where pages are reserved and physically backed on-the-fly by the page fault handler only upon first access.
- **Multitasking & Synchronization:** Kernel and user thread creation, context switching, and round-robin scheduling. Hardware interactions (like the ATA driver) avoid busy-looping by utilizing thread-blocking wait queues that yield the CPU until the PIC fires an IRQ.
- **User space transition:** Small syscall layer plus a freestanding `init.elf` program. The kernel builds a secure IRETQ stack frame to drop the CPU into Ring 3, dynamically updating the TSS `rsp0` pointer on context switches to ensure every user thread maintains a unique, isolated kernel stack for trap handling.
- **Storage and filesystem:** Interrupt-driven ATA block device driver, MBR parsing, ext2 detection, inode/directory traversal, and file reads.
- **Testing & Verification:** Automated traversal of the ext2 filesystem that computes the MD5 hash of read files to prove driver accuracy and data integrity against disk corruption.
- **Program loading:** ELF64 loader that parses program headers, maps segments directly into a distinct user address space, and starts execution.

## Memory Architecture

The kernel uses an explicit virtual address layout defined in
[`memory.h`](/src/include/kernel/memory.h):

- `0x000000000000`: identity-mapped physical memory window
- `0x010000000000`: kernel heap base
- `0x0F0000000000`: kernel stack region
- `0x100000000000`: user virtual address base
- `0x110000000000`: user stack base

This layout matters because it keeps the project organized around real address
space boundaries instead of treating all execution as one undifferentiated
memory region.

The memory subsystem currently includes:

- A physical frame allocator built from the multiboot memory map, with kernel
  ELF regions carved out so reserved frames are not handed back as free memory
- Four-level x86-64 page-table walking and allocation
- Demand paging support, where virtual pages are marked as reserved and backed
  by physical frames lazily in the page fault handler on first touch
- Separate user page-table roots created with `MMU_create_user_p4()`
- User mappings marked with the user-accessible bit, while kernel mappings
  remain supervisor-only

When a user program is launched, the ELF loader creates a new user `cr3`,
copies the kernel portion of the PML4 into the new address space so syscalls
and traps can still be serviced, maps the program segments into the user
region, allocates a user stack, and starts execution in ring 3 via `iretq`
state prepared by the scheduler. That separation is one of the biggest
indicators of project maturity in the current codebase.

## Project Layout

```text
src/kernel/                  Architecture-agnostic core kernel subsystems
src/x86_64/                  Architecture-specific subsystems and boot code
src/x86_64/interrupts/       Interrupt, PIC, and IDT
src/x86_64/memory/           Physical and virtual memory management
src/x86_64/multitask/        Scheduler, process/thread setup, and syscalls
src/x86_64/drivers/          VGA, ATA, PIT, PS/2 keyboard, serial
userspace/                   Tiny freestanding user program and linker script
klibc/                       Minimal C library support used by the kernel
make_image.sh                Disk image creation, partitioning, filesystem setup, GRUB install
gdbinit                      Convenience script for debugging with GDB
```

## Building and Running

### Prerequisites

You will need a Linux development environment with:

- `nasm`
- `qemu-system-x86_64`
- `grub-install`
- `parted`
- `losetup`
- `mkfs.ext2`
- `gdb`
- `sudo`
- an `x86_64-elf-gcc` cross-compiler

By default the `Makefile` expects the cross-compiler at:

```bash
~/cross/bin/x86_64-elf-gcc
```

### Build

```bash
make
```

This builds:

- the kernel binary at `build/kernel-x86_64.bin`
- the user program at `build/init.elf`

### Run

```bash
chmod +x make_image.sh
make run
```

`make run` will:

1. Build a raw disk image
2. Partition it with an MBR layout
3. Create an ext2 filesystem
4. Install GRUB
5. Copy the kernel and `init.elf` into the image
6. Boot the image in QEMU with serial output enabled

Notes:

- `make_image.sh` uses loop devices, filesystem tools, mounts, and `sudo`
- On most systems that means image creation requires elevated privileges
- QEMU is launched with `-s`, so it automatically opens a GDB server on port `1234`

## Debugging

Start the kernel in one terminal:

```bash
make run
```

Then attach from another terminal:

```bash
gdb -x gdbinit
```

The provided [`gdbinit`](/home/james/CPE454-kernel/gdbinit) file configures:

- x86-64 Intel syntax
- the kernel symbol file
- a remote target at `localhost:1234`
- TUI mode

If you prefer to enter the commands manually:

```gdb
set arch i386:x86-64:intel
symbol-file build/kernel-x86_64.bin
target remote localhost:1234
tui enable
```

Useful places to set early breakpoints include:

- `kernel_main`
- `interrupts_init`
- `MMU_init`
- `PROC_create_uthread`
- `elf_load`

## Further Reading

I plan to add implementation writeups and notes for the subsystems below:

- [Boot Process and Entering Long Mode](#)
- [Interrupts, IDT, and Fault Handling](#)
- [Physical Memory and Paging Design](#)
- [Scheduler and Context Switching](#)
- [ATA, MBR Parsing, and ext2 Integration](#)
- [ELF Loading and Starting User Space](#)
- [Lessons Learned While Building the Kernel](#)

## Status

This is an active personal learning project and a long-term systems playground.
The current codebase already demonstrates core operating-system concepts end to
end, and I plan to continue extending it with more robust process management,
filesystem capabilities, and stronger debugging/testing tools.
