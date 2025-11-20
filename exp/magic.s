#ifdef __APPLE__
        #define FNAME _swap_rfiles
#else                           /* everyone else */
        #define FNAME swap_rfiles
#endif

        .text
        .globl FNAME
        #ifndef __APPLE__
        .type  swap_rfiles, @function
        #endif
  FNAME:
        # void swap_rfiles(rfile *old, rfile *new)
        #
        # "old" will be in rdi
        # "new" will be in rsi
        #
        pushq %rbp              # set up a frame pointer
        movq %rsp,%rbp

        # save the old context (if old != NULL)
        cmpq    $0,%rdi
        je load

        movq %rax,   (%rdi)     # store rax into old->rax so we can use it

        # Now store the Floating Point State
        leaq 128(%rdi),%rax     # get the address
        fxsave (%rax)

        movq %rbx,  8(%rdi)     # now the rest of the registers
        movq %rcx, 16(%rdi)     # etc.
        movq %rdx, 24(%rdi)
        movq %rsi, 32(%rdi)
        movq %rdi, 40(%rdi)
        movq %rbp, 48(%rdi)
        movq %rsp, 56(%rdi)
        movq %r8,  64(%rdi)
        movq %r9,  72(%rdi)
        movq %r10, 80(%rdi)
        movq %r11, 88(%rdi)
        movq %r12, 96(%rdi)
        movq %r13,104(%rdi)
        movq %r14,112(%rdi)
        movq %r15,120(%rdi)

        # load the new one (if new != NULL)
load:   cmpq    $0,%rsi
        je done

        # First restore the Floating Point State
        leaq 128(%rsi),%rax     # get the address
        fxrstor (%rax)

        movq    (%rsi),%rax     # retreive rax from new->rax
        movq   8(%rsi),%rbx     # etc.
        movq  16(%rsi),%rcx
        movq  24(%rsi),%rdx
        movq  40(%rsi),%rdi
        movq  48(%rsi),%rbp
        movq  56(%rsi),%rsp
        movq  64(%rsi),%r8 
        movq  72(%rsi),%r9 
        movq  80(%rsi),%r10
        movq  88(%rsi),%r11
        movq  96(%rsi),%r12
        movq 104(%rsi),%r13
        movq 112(%rsi),%r14
        movq 120(%rsi),%r15
        movq  32(%rsi),%rsi     # must do rsi last, since it's our pointer

done:   leave
        ret