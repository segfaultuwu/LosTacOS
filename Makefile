include make/config.mk
include make/paths.mk
include make/flags.mk

include make/version.mk
include make/kernel.mk
include make/libc.mk
include make/rootfs.mk
include make/iso.mk
include make/toolchain.mk
include make/clean.mk

include make/init.mk


.PHONY: all

all: iso


-include $(DEP)
