# x86_64 Kernel Project

use $make run


If permission error occurs wheb trying to $make run then ensure the shell script 
is executable. $chmod +x make_image.sh

To debug:

gdb
set arch i386:x86-64:intel
symbol-file build/kernel-x86_64.bin
target remote localhost:1234


There are sixteen 64-bit registers in x86-64: %rax, %rbx, %rcx, %rdx, %rdi, %rsi, %rbp,
%rsp, and %r8-r15. Of these, %rax, %rcx, %rdx, %rdi, %rsi, %rsp, and %r8-r11 are
considered caller-save registers, meaning that they are not necessarily saved across function
calls. By convention, %rax is used to store a function’s return value, if it exists and is no more
than 64 bits long. (Larger return types like structs are returned using the stack.) Registers %rbx,
%rbp, and %r12-r15 are callee-save registers, meaning that they are saved across function
calls. Register %rsp is used as the stack pointer, a pointer to the topmost element in the stack.
Additionally, %rdi, %rsi, %rdx, %rcx, %r8, and %r9 are used to pass the first six integer
or pointer parameters to called functions. Additional parameters (or large parameters such as
structs passed by value) are passed on the stack.
In 32-bit x86, the base pointer (formerly %ebp, now %rbp) was used to keep track of the base of
the current stack frame, and a called function would save the base pointer of its caller prior to
updating the base pointer to its own stack frame. With the advent of the 64-bit architecture, this
has been mostly eliminated, save for a few special cases when the compiler cannot determine
ahead of time how much stack space needs to be allocated for a particular function

https://cs.brown.edu/courses/cs033/docs/guides/x64_cheatsheet.pdf
https://users.ece.utexas.edu/~adnan/gdb-refcard.pdf


