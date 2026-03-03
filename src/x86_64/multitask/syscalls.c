#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>
#include <kernel/multitask.h>
#include <kernel/syscall.h>
#include <kernel/ps2_kbd.h>


#define SYSCALL_ISR_VECTOR 128
/* temp kexit fix */
#define KEXIT_ISR_VECTOR_TEMP 0x81


static void ISR128_syscall(int vector, int err, void *rsp);
static void ISR129_kexit(int vector, int err, void *rsp);

static uint64_t sys_getc(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, 
    uint64_t a5, uint64_t a6);
static uint64_t sys_putc(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, 
    uint64_t a5, uint64_t a6);


/* Table of syscall function pointers. Indexed using the syscall num */
static struct syscall_entry syscall_table[NUM_SYSCALLS];

/*******************************************************************************
 * SYSCALL API
 ******************************************************************************/
void syscall_init()
{
    /* register all syscalls as a trap on vector 128 */
    register_interrupt(SYSCALL_ISR_VECTOR, 0, TYPE_INTRGATE, 3, ISR128_syscall, NULL);

    /* this temp interrupt will allow the kexit syscall to run on IST 3 
     * The generic syscalls may not run on IST3 since it causes issues with 
     * yield saving the wrong rsp. Once we have a cleanup thread remove this
     * vector and call kexit using the syscall vector on 0x80
     */
    register_interrupt(KEXIT_ISR_VECTOR_TEMP, 3, TYPE_INTRGATE, 3, ISR129_kexit, NULL);

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
        /* store return into RAX */
        regs->rax = syscall_table[sys_num].handler(a1, a2, a3, a4, a5, a6);
    } else {
        printk("the requested syscall (#%d) does not exist!\n", (int)sys_num);
        printk("halting...\n");
        __asm__("hlt");
    }

    // TODO: return the specific syscalls return code... regs->rax = ret 
    // not strictly needed since our syscalls can print and terminate for errors
}

static void ISR129_kexit(int vector, int err, void *rsp)
{
    /* sanity */
    if (syscall_table[SYS_KEXIT_NUM].handler != NULL) {
        /* just call it directly */
        syscall_table[SYS_KEXIT_NUM].handler(0, 0, 0, 0, 0, 0);
    } else {
        printk("kexit syscall handler not registered!\n");
        __asm__("hlt");
    }
}

/* might move these to their own file. Just generic syscalls */
/* getc */
static uint64_t sys_getc(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, 
    uint64_t a5, uint64_t a6)
{
    return (uint64_t)keyboard_getchar();
}

/* putc */
static uint64_t sys_putc(uint64_t c, uint64_t a2, uint64_t a3, uint64_t a4, 
    uint64_t a5, uint64_t a6)
{
    /* just calls printk since putchar is not exposed */
    printk("%c", (char)c);
    return 0;
}