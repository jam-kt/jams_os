global isr_handler

extern c_isr_handler

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
    
    ; save the vector number and error code according to System V AMD64 ABI
    ; move past the recently pushed items in the stack (72 bytes worth) to 
    ; reach the vector num and error code the stub pushed
    mov rsi, [rsp + 80]     ; error
    mov rdi, [rsp + 72]     ; vector
    mov rdx, rsp            ; pass the stack pointer into 3rd C function arg

    call c_isr_handler      ; c_irq_handler(int irq, unsigned err)

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

    iretq