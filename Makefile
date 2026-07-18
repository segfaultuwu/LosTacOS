CXX = g++
LD = ld
AS = nasm

CXXFLAGS = -std=c++23 -ffreestanding -O2 -Wall -Wextra \
           -fno-exceptions -fno-rtti \
           -mno-red-zone -mcmodel=kernel \
					 -fno-pic -I./include/ -I./build/generated/ -fno-builtin \
					 -fno-stack-protector -mno-sse -mno-sse2 \
					 -mno-mmx -mno-avx -mgeneral-regs-only

CFLAGS = -std=c23 -ffreestanding -O2 -Wall -Wextra \
         -fno-builtin -Iinclude/ \
         -mno-red-zone -mno-sse -mno-sse2 \
         -mno-mmx -mno-avx -mgeneral-regs-only

LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000

SRC_CPP = $(shell find src -name "*.cpp")
SRC_C   = $(shell find src -name "*.c")
SRC_ASM = $(shell find src -name "*.asm")

OBJ_CPP = $(SRC_CPP:.cpp=.cpp.o)
OBJ_ASM = $(SRC_ASM:.asm=.asm.o)
OBJ_C   = $(SRC_C:.c=.c.o)

OBJ = $(OBJ_CPP) $(OBJ_C) $(OBJ_ASM)

KERNEL = build/kernel.elf
ISO = build/LosTacOS-x86_64.iso

all: iso

build:
	mkdir -p build

%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.c.o: %.c
	gcc $(CFLAGS) -c $< -o $@

%.asm.o: %.asm
	$(AS) -f elf64 $< -o $@

$(KERNEL): build version $(OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL) $(OBJ)

iso: $(KERNEL)
	mkdir -p build/isodir/boot/grub
	cp $(KERNEL) build/isodir/boot/kernel.elf
	cp assets/font.psf build/isodir/boot/font.psf

	echo 'set timeout=0' > build/isodir/boot/grub/grub.cfg
	echo 'set default=0' >> build/isodir/boot/grub/grub.cfg
	echo 'menuentry "LosTacOS" {' >> build/isodir/boot/grub/grub.cfg
	echo '  multiboot2 /boot/kernel.elf' >> build/isodir/boot/grub/grub.cfg
	echo '  module2 /boot/font.psf font.psf' >> build/isodir/boot/grub/grub.cfg
	echo '  boot' >> build/isodir/boot/grub/grub.cfg
	echo '}' >> build/isodir/boot/grub/grub.cfg

	grub-mkrescue -o $(ISO) build/isodir

version:
	bash ./tools/genver.sh

run: iso
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-serial stdio \
		-no-reboot \
		-no-shutdown

clean:
	rm -rf build $(OBJ_CPP) $(OBJ_ASM) $(OBJ_C)
