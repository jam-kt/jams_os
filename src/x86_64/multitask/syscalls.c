#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>
#include <kernel/multitask.h>
#include <kernel/syscall.h>


#define SYSCALL_ISR_VECTOR 128


static void ISR128_syscall(int vector, int err, void *rsp);


/* Table of syscall function pointers. Indexed using the syscall num */
static struct syscall_entry syscall_table[NUM_SYSCALLS];


/* 
 * in retrospect I am not sure if we need such an API for syscalls...
 * it would be possible to just have one interrupt vector for every syscall
 */

/*******************************************************************************
 * SYSCALL API
 ******************************************************************************/
void syscall_init()
{
    /* register all syscalls as a trap on vector 128 */
    // TODO: very likely need to make the gate user-callable via DPL=3
    // otherwise we would fault when trying to syscall from user space
    // requires modifying the interrupts api to allow an extra arg
    register_interrupt(SYSCALL_ISR_VECTOR, 3, TYPE_TRAPGATE, ISR128_syscall, NULL);
}

void register_syscall(int sys_num, sys_t handler)
{
    if (sys_num < 0 || sys_num >= NUM_SYSCALLS || syscall_table[sys_num].handler) {
        printk("Tried to register a syscall using a bad number: %d", sys_num);
        return;
    }

    syscall_table[sys_num].handler = handler;
}


/* 
 * generic syscall entry point to be registered on interrupt trap vector 0x80.
 * This should run on a different stack (no rug pull for kexit syscall)
 */
static void ISR128_syscall(int vector, int err, void *rsp)
{
    /* use passed rsp to get syscall args from stack */
    struct {
        uint64_t r15, r14, r13, r12, rbp, rbx, 
                    r11, r10, r9, r8, rdi, rsi, rdx, rcx, rax;
    } *regs = rsp;
    
    /* match the AMD64 ABI */
    uint64_t sys_num = regs->rax;
    uint64_t a1 = regs->rdi;
    uint64_t a2 = regs->rsi;
    uint64_t a3 = regs->rdx;
    uint64_t a4 = regs->rcx;
    uint64_t a5 = regs->r8;
    uint64_t a6 = regs->r9;

    if ((vector != SYSCALL_ISR_VECTOR) || (err != 0)) {
        printk("unexpected entry into the syscall ISR\n");
        return;
    }

    /* dispatch to the given syscall number or default to printing an error */
    if ((sys_num < NUM_SYSCALLS) && (syscall_table[sys_num].handler != NULL)) {
        syscall_table[sys_num].handler(a1, a2, a3, a4, a5, a6);
    } else {
        printk("the requested syscall (#%d) does not exist!\n", (int)sys_num);
        printk("halting...\n");
        __asm__("hlt");
    }

    // TODO: return the specific syscalls return code... regs->rax = ret 
    // not strictly needed since our syscalls can print and terminate for errors
}