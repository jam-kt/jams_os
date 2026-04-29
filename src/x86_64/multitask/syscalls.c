#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>
#include <kernel/multitask.h>
#include <kernel/syscall.h>
#include <kernel/ps2_kbd.h>


#define SYSCALL_ISR_VECTOR 128


static void ISR128_syscall(int vector, int err, void *rsp);

static uint64_t sys_getc(struct syscall_frame *frame);
static uint64_t sys_putc(struct syscall_frame *frame);


/* Table of syscall function pointers. Indexed using the syscall num */
static struct syscall_entry syscall_table[NUM_SYSCALLS];

/*******************************************************************************
 * SYSCALL API
 ******************************************************************************/
void syscall_init()
{
    /* register all syscalls as a trap on vector 128 */
    /* ^^ need to do some bug fixes first, not thread safe rn */
    register_interrupt(SYSCALL_ISR_VECTOR, 0, TYPE_INTRGATE, 3, ISR128_syscall, NULL);

    /* register generic IO syscalls */
    register_syscall(SYS_GETC_NUM, sys_getc);
    register_syscall(SYS_PUTC_NUM, sys_putc);
}

void register_syscall(int sys_num, sys_t handler)
{
    if (sys_num < 0 || sys_num >= NUM_SYSCALLS || syscall_table[sys_num].handler) {
        printk("Tried to register a syscall using a bad number: %d", sys_num);
        return;
    }

    syscall_table[sys_num].handler = handler;
}


/* generic syscall entry point to be registered on interrupt trap vector 0x80 */
static void ISR128_syscall(int vector, int err, void *rsp)
{
    struct syscall_frame *frame = rsp;
    uint64_t sys_num = frame->rax;

    if ((vector != SYSCALL_ISR_VECTOR) || (err != 0)) {
        printk("unexpected entry into the syscall ISR\n");
        return;
    }

    /* dispatch to the given syscall number or default to printing an error */
    if ((sys_num < NUM_SYSCALLS) && (syscall_table[sys_num].handler != NULL)) {
        /* store return into RAX */
        frame->rax = syscall_table[sys_num].handler(frame);
    } else {
        printk("the requested syscall (#%d) does not exist!\n", (int)sys_num);
        printk("halting...\n");
        __asm__("hlt");
    }
}

/* might move these to their own file. Just generic syscalls */
/* getc */
static uint64_t sys_getc(struct syscall_frame *frame)
{
    (void)frame;
    return (uint64_t)keyboard_getchar();
}

/* putc */
static uint64_t sys_putc(struct syscall_frame *frame)
{
    /* just calls printk since putchar is not exposed */
    printk("%c", (char)frame->rdi);
    return 0;
}