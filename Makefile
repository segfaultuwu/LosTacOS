CXX = g++
CC  = gcc
LD = ld
AS = nasm
TAR = tar

CXXFLAGS = -std=c++23 -ffreestanding -O2 -Wall -Wextra \
           -fno-exceptions -fno-rtti \
           -mno-red-zone -mcmodel=kernel \
           -fno-pic \
           -I./include/ \
           -I./build/generated/ \
           -fno-builtin \
           -fno-stack-protector \
           -mno-sse -mno-sse2 \
           -mno-mmx -mno-avx \
           -mgeneral-regs-only

CFLAGS = -std=c23 -ffreestanding -O2 -Wall -Wextra \
         -fno-builtin \
         -Iinclude \
         -mno-red-zone \
         -mno-sse -mno-sse2 \
         -mno-mmx -mno-avx \
         -mgeneral-regs-only

LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000


SRC_CPP := $(shell find src -name "*.cpp")
SRC_C   := $(shell find src -name "*.c")
SRC_ASM := $(shell find src -name "*.asm")

OBJ_CPP := $(SRC_CPP:.cpp=.cpp.o)
OBJ_C   := $(SRC_C:.c=.c.o)
OBJ_ASM := $(SRC_ASM:.asm=.asm.o)

OBJ := $(OBJ_CPP) $(OBJ_C) $(OBJ_ASM)


KERNEL = build/kernel.elf
ISO = build/LosTacOS-x86_64.iso

ROOTFS = build/rootfs
TARFS = build/rootfs.tar


.PHONY: all iso clean run user tarfs version


all: iso


build:
	mkdir -p build


version:
	bash ./tools/genver.sh


%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


%.asm.o: %.asm
	$(AS) -f elf64 $< -o $@



$(KERNEL): $(OBJ) version | build
	$(LD) $(LDFLAGS) -o $@ $(OBJ)



bin/hello: bin/hello.asm
	$(AS) -f elf64 $< -o bin/hello.o
	$(LD) -o $@ bin/hello.o



user: bin/hello



tarfs: user
	rm -rf $(ROOTFS)

	mkdir -p $(ROOTFS)/usr/bin
	mkdir -p $(ROOTFS)/usr/lib
	mkdir -p $(ROOTFS)/dev
	mkdir -p $(ROOTFS)/proc
	mkdir -p $(ROOTFS)/sys
	mkdir -p $(ROOTFS)/tmp
	mkdir -p $(ROOTFS)/bin

	cp bin/hello $(ROOTFS)/bin/hello

	$(TAR) --format=ustar \
		-cf $(TARFS) \
		-C $(ROOTFS) .



iso: $(KERNEL) tarfs

	rm -rf build/isodir

	mkdir -p build/isodir/boot/grub

	cp $(KERNEL) build/isodir/boot/kernel.elf
	cp $(TARFS) build/isodir/boot/rootfs.tar
	cp assets/font.psf build/isodir/boot/font.psf


	echo 'set timeout=0' > build/isodir/boot/grub/grub.cfg
	echo 'set default=0' >> build/isodir/boot/grub/grub.cfg
	echo 'menuentry "LosTacOS" {' >> build/isodir/boot/grub/grub.cfg
	echo '  multiboot2 /boot/kernel.elf' >> build/isodir/boot/grub/grub.cfg
	echo '  module2 /boot/font.psf font.psf' >> build/isodir/boot/grub/grub.cfg
	echo '  module2 /boot/rootfs.tar rootfs.tar' >> build/isodir/boot/grub/grub.cfg
	echo '  boot' >> build/isodir/boot/grub/grub.cfg
	echo '}' >> build/isodir/boot/grub/grub.cfg


	grub-mkrescue -o $(ISO) build/isodir



run: iso
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-serial stdio \
		-no-reboot \
		-no-shutdown



clean:
	rm -rf build
	rm -f $(OBJ_CPP) $(OBJ_C) $(OBJ_ASM)
	rm -f bin/hello bin/hello.o
