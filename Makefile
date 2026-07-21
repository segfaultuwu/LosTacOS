# =========================
# Toolchain
# =========================

CROSS ?=

CXX = $(CROSS)g++
CC  = $(CROSS)gcc
LD  = $(CROSS)ld
AR  = $(CROSS)ar

AS = nasm
TAR = tar

LTOSCC = ./tools/ltoscc


# =========================
# Paths
# =========================

BUILD = build
OBJDIR = $(BUILD)/obj

KERNEL = $(BUILD)/kernel.elf
ISO = $(BUILD)/LosTacOS-x86_64.iso

ROOTFS = $(BUILD)/rootfs
TARFS = $(BUILD)/rootfs.tar


# =========================
# Flags
# =========================

COMMON_FLAGS = \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-mno-red-zone \
	-mno-sse \
	-mno-sse2 \
	-mno-mmx \
	-mno-avx \
	-mgeneral-regs-only


DEBUG ?= 0

ifeq ($(DEBUG),1)
OPT = -O0 -g
else
OPT = -O2
endif


CXXFLAGS = \
	-std=c++23 \
	$(COMMON_FLAGS) \
	-fno-exceptions \
	-fno-rtti \
	-mcmodel=kernel \
	-fno-pic \
	-Iinclude \
	-I$(BUILD)/generated \
	$(OPT) \
	-MMD -MP


CFLAGS = \
	-std=c23 \
	$(COMMON_FLAGS) \
	-Iinclude \
	$(OPT) \
	-MMD -MP


LDFLAGS = \
	-T linker.ld \
	-nostdlib \
	-z max-page-size=0x1000


# =========================
# Sources
# =========================

SRC_CPP := $(shell find src -name "*.cpp")
SRC_C   := $(shell find src -name "*.c")
SRC_ASM := $(shell find src -name "*.asm")


OBJ_CPP := $(patsubst src/%.cpp,$(OBJDIR)/%.cpp.o,$(SRC_CPP))
OBJ_C   := $(patsubst src/%.c,$(OBJDIR)/%.c.o,$(SRC_C))
OBJ_ASM := $(patsubst src/%.asm,$(OBJDIR)/%.asm.o,$(SRC_ASM))


OBJ = $(OBJ_CPP) $(OBJ_C) $(OBJ_ASM)

DEP = $(OBJ:.o=.d)



.PHONY: all iso clean run user tarfs libc version


all: iso



# =========================
# Build dir
# =========================

$(BUILD):
	mkdir -p $(OBJDIR)



version:
	bash ./tools/genver.sh



# =========================
# Kernel compile
# =========================

$(OBJDIR)/%.cpp.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@


$(OBJDIR)/%.c.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


$(OBJDIR)/%.asm.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@



# =========================
# Kernel
# =========================

$(KERNEL): version $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)



# =========================
# User libc
# =========================

LIBC_ASM := $(shell find libc -name "*.asm")
LIBC_C   := $(shell find libc -name "*.c")

LIBC_OBJ := $(LIBC_ASM:.asm=.o) $(LIBC_C:.c=.o)



%.o: %.asm
	$(AS) -f elf64 $< -o $@

libc/%.o: libc/%.c
	$(CC) \
		-std=c23 \
		-ffreestanding \
		-fno-builtin \
		-fno-stack-protector \
		-mno-red-zone \
		-mgeneral-regs-only \
		-Ilibc/include \
		-c $< \
		-o $@


libc: $(LIBC_OBJ)

	mkdir -p $(ROOTFS)/usr/lib

	$(AR) rcs \
	$(ROOTFS)/usr/lib/libc.a \
	$(LIBC_OBJ)

	mkdir -p $(ROOTFS)/lib

	cp libc/src/crt0.o \
	$(ROOTFS)/lib/



# =========================
# User programs
# =========================

bin/hello: bin/hello.c libc
	$(LTOSCC) $< -o $@



user: bin/hello



# =========================
# RootFS
# =========================

rootfs_dirs:
	rm -rf $(ROOTFS)

	mkdir -p \
	$(ROOTFS)/bin \
	$(ROOTFS)/lib \
	$(ROOTFS)/usr/include \
	$(ROOTFS)/usr/lib \
	$(ROOTFS)/dev \
	$(ROOTFS)/proc \
	$(ROOTFS)/sys \
	$(ROOTFS)/tmp



headers: rootfs_dirs
	cp -r libc/include/* \
	$(ROOTFS)/usr/include/



tarfs: headers libc user

	cp bin/hello \
	$(ROOTFS)/bin/init


	$(TAR) \
	--format=ustar \
	-cf $(TARFS) \
	-C $(ROOTFS) .



# =========================
# ISO
# =========================

iso: $(KERNEL) tarfs

	rm -rf $(BUILD)/isodir

	mkdir -p \
	$(BUILD)/isodir/boot/grub


	cp $(KERNEL) \
	$(BUILD)/isodir/boot/kernel.elf


	cp $(TARFS) \
	$(BUILD)/isodir/boot/rootfs.tar


	cp assets/font.psf \
	$(BUILD)/isodir/boot/font.psf


	cp cfg/grub.cfg \
	$(BUILD)/isodir/boot/grub/grub.cfg


	grub-mkrescue \
	-o $(ISO) \
	$(BUILD)/isodir



# =========================
# Run
# =========================

run: iso
	qemu-system-x86_64 \
	-cdrom $(ISO) \
	-serial stdio \
	-no-reboot \
	-no-shutdown



debug: iso
	qemu-system-x86_64 \
	-cdrom $(ISO) \
	-serial stdio \
	-s -S



# =========================
# Clean
# =========================

clean:
	rm -rf $(BUILD)
	rm -f bin/hello
	rm -f $(LIBC_OBJ)


-include $(DEP)
