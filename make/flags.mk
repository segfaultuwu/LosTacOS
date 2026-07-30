COMMON_FLAGS = \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-mno-red-zone \
	-mno-mmx \
	-mno-avx \
	-mgeneral-regs-only

LIBC_FLAGS = \
	-msse \
	-msse2 \
	-std=c23 \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-mno-red-zone \
	-Ilibc/include


DEBUG ?= 0

ifeq ($(DEBUG),1)
OPT=-O0 -g
else
OPT=-O2
endif


CXXFLAGS = \
	-std=c++23 \
	$(COMMON_FLAGS) \
	-fno-exceptions \
	-fno-rtti \
	-mcmodel=kernel \
	-fno-pic \
	-Iinclude \
	-Ilibc/include \
	-I$(BUILD)/generated \
	$(OPT) \
	-MMD -MP


CFLAGS = \
	-std=c23 \
	$(COMMON_FLAGS) \
	-Iinclude \
	-Ilibc/include \
	$(OPT) \
	-MMD -MP


LDFLAGS = \
	-T linker.ld \
	-nostdlib \
	-z max-page-size=0x1000
