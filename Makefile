# Recursively find all C and ASM files. Object files go in build directory
src_c_files := $(shell find src -type f -name '*.c')
src_obj_files := $(patsubst %.c,build/%.o,$(src_c_files))

klibc_c_files  := $(shell find klibc -type f -name '*.c')
klibc_obj_files:= $(patsubst %.c,build/%.o,$(klibc_c_files))

# Combine all C source files.
c_obj_files := $(src_obj_files) $(klibc_obj_files)

all_asm_files := $(shell find src -type f -name '*.asm')
asm_obj_files := $(patsubst %.asm,build/%.o,$(all_asm_files))

arch ?= x86_64
kernel_bin := build/kernel-$(arch).bin
img := build/boot-$(arch).img

CC := ~/cross/bin/$(arch)-elf-gcc
CFLAGS := -Wall -Werror -std=gnu99 -g -c -mno-red-zone -ffreestanding
CFLAGS += -Isrc/include -Iklibc/include -masm=intel

linker_script := src/$(arch)/linker.ld
grub_cfg := src/$(arch)/grub.cfg

MAKE_IMAGE_SCRIPT := ./make_image.sh

# userspace stuff
user_bins := build/init.elf build/echo.elf
user_libs := userspace/lib/crt0.c userspace/lib/syscall.c
user_linker_script := userspace/user_linker.ld

.PHONY: all clean run img

all: $(kernel_bin) $(user_bins)

clean:
	@rm -rf build

run: $(img)
	@qemu-system-$(arch) -s -drive format=raw,file=$(img) -serial stdio

img: $(img)

$(img): $(kernel_bin) $(user_bins)
	@$(MAKE_IMAGE_SCRIPT) $(img) $(kernel_bin) $(grub_cfg) $(user_bins)

$(kernel_bin): $(asm_obj_files) $(c_obj_files) $(linker_script)
	@ld -n -T $(linker_script) -o $(kernel_bin) $(asm_obj_files) $(c_obj_files)

# Rule to compile user-space programs
build/%.elf: userspace/%.c $(user_libs) $(user_linker_script)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Werror -std=gnu99 -g -mno-red-zone -ffreestanding \
	-Iuserspace/include -nostdlib -mcmodel=large -T $(user_linker_script) \
	$(user_libs) $< -o $@

# Generic rule for compiling C files that preserves their relative path.
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

# Generic rule for assembling .asm files.
build/%.o: %.asm
	@mkdir -p $(dir $@)
	@nasm -f elf64 -g -F dwarf $< -o $@
