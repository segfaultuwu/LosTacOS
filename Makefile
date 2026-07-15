CXX = g++
LD = ld
AS = nasm

CXXFLAGS = -std=c++23 -ffreestanding -O2 -Wall -Wextra \
           -fno-exceptions -fno-rtti \
           -mno-red-zone -mcmodel=kernel \
					 -fno-pic

LDFLAGS = -T linker.ld -nostdlib

SRC_CPP = $(shell find src -name "*.cpp")
SRC_ASM = $(shell find src -name "*.asm")

OBJ_CPP = $(SRC_CPP:.cpp=.o)
OBJ_ASM = $(SRC_ASM:.asm=.o)

OBJ = $(OBJ_CPP) $(OBJ_ASM)

KERNEL = build/kernel.bin
ISO = build/LosTacOS-x86_64.iso

all: iso

build:
	mkdir -p build

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) -f elf64 $< -o $@

$(KERNEL): build $(OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL) $(OBJ)

iso: $(KERNEL)
	mkdir -p build/isodir/boot/grub
	cp $(KERNEL) build/isodir/boot/kernel.bin

	echo 'set timeout=0' > build/isodir/boot/grub/grub.cfg
	echo 'set default=0' >> build/isodir/boot/grub/grub.cfg
	echo 'menuentry "LosTacOS" {' >> build/isodir/boot/grub/grub.cfg
	echo '  multiboot2 /boot/kernel.bin' >> build/isodir/boot/grub/grub.cfg
	echo '  boot' >> build/isodir/boot/grub/grub.cfg
	echo '}' >> build/isodir/boot/grub/grub.cfg

	grub-mkrescue -o $(ISO) build/isodir

run: iso
	qemu-system-x86_64 -cdrom $(ISO)

clean:
	rm -rf build $(OBJ_CPP) $(OBJ_ASM)
