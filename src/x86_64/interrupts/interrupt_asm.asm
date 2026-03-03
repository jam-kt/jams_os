global isr_handler

extern c_isr_handler
extern curr_proc
extern next_proc

extern switch_mmu_and_tss

%define PROC_STATE_RSP_OFFSET 104 ; offset for RSP field (proc->state.rsp)

section .text
bits 64
isr_handler:
    ; save scratch registers (normally caller saved)
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; save rest of minimal register set (see multitasking)
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; save the vector number and error code according to AMD64 ABI
    ; move past the recently pushed items in the stack (120 bytes worth) to 
    ; reach the vector num and error code the stub pushed
    mov rsi, [rsp + 128]    ; error
    mov rdi, [rsp + 120]    ; vector
    mov rdx, rsp            ; pass the stack pointer into 3rd C function arg

    call c_isr_handler      ; c_irq_handler(int irq, unsigned err)

    ; compare curr and next processes and context switch if they are different
    mov rbx, [rel curr_proc]    ; load ptr to curr_proc using RIP relative mode
    mov r12, [rel next_proc]
    test r12, r12               ; if next_proc == NULL skip context switch
    jz  .restore
    cmp r12, rbx                ; if curr == next proc skip context switch
    je  .restore               
    test rbx, rbx               ; check if curr_proc == NULL, happens on kexit
    jz  .swap_context

    mov [rbx + PROC_STATE_RSP_OFFSET], rsp  ; save sp into curr's context struct
.swap_context:
    mov rdi, r12                            ; to pass next_proc to function
    call switch_mmu_and_tss                 ; see idt_tss.c
    mov rsp, [r12 + PROC_STATE_RSP_OFFSET]  ; move next's sp into RSP
    mov [rel curr_proc], r12                ; curr_proc = next_proc
    mov qword [rel next_proc], 0            ; next_proc = 0

.restore:
    ; save rest of minimal register set (see multitasking)
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; restore scratch registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    ; move sp past the vector and error code
    add rsp, 16

.debug_restore:
    nop
    iretq
