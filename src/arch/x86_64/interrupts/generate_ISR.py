# generate 256 generic ISR stubs

# Interrupts 0-31 are exceptions. Some exceptions push an error code onto the
# stack. These include interrupts on vectors 8, 10, 11, 12, 13, 14, 17, 21, 29,
# and 30. Interrupts that do not push an error code are made to pushh a dummy
# code so that we can use a single common ASM handler
# Note: exceptions 21, 29, 30 should never happen (need certain hardware support
# to be enabled first)

OUTPUT = "generated_interrupts.asm"

def generate_isr_stubs():
    with open(OUTPUT, 'w') as f:
        f.write("extern isr_handler\n")
        f.write("section .text\n")
        f.write("bits 64\n\n")
        for i in range(256):
            f.write(f"global isr{i}\n")
            f.write(f"isr{i}:\n")

            if i in [8, 10, 11, 12, 13, 14, 17, 21, 29, 30]:
                f.write(f"    push {i}\n")
                f.write(f"    nop\n")           # padding so that each stub is 
                f.write(f"    nop\n")           # the same size (9 bytes)
            else:
                f.write(f"    push 0\n")        # dummy error code
                f.write(f"    push {i}\n")
            
            f.write("    jmp isr_handler\n\n")

    print(f"Done")

if __name__ == "__main__":
    generate_isr_stubs()
