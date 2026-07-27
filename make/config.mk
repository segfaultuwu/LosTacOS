CROSS ?=

CXX = $(CROSS)g++
CC  = $(CROSS)gcc
LD  = $(CROSS)ld
AR  = $(CROSS)ar

AS = nasm
TAR = tar

LTOSCC = ./tools/ltoscc
LTOSLD = ./tools/ltosld


BUILD = build
OBJDIR = $(BUILD)/obj

KERNEL = $(BUILD)/kernel.elf
ISO = $(BUILD)/LosTacOS-x86_64.iso

ROOTFS = $(BUILD)/rootfs
ROOTDIR = root

TARFS = $(BUILD)/rootfs.tar

LTOS_SYSROOT = $(HOME)/.ltos/toolchain/sysroot
