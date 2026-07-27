LIBC_ASM=$(shell find libc -name "*.asm")
LIBC_C=$(shell find libc -name "*.c")


LIBC_OBJ= \
$(patsubst libc/%.asm,$(BUILD)/libc/%.o,$(LIBC_ASM)) \
$(patsubst libc/%.c,$(BUILD)/libc/%.o,$(LIBC_C))



$(BUILD)/libc/%.o: libc/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@



$(BUILD)/libc/%.o: libc/%.c
	@mkdir -p $(dir $@)

	$(CC) \
	$(LIBC_FLAGS) \
	-c $< \
	-o $@



libc: $(LIBC_OBJ)

	mkdir -p $(ROOTFS)/usr/lib

	$(AR) rcs \
	$(ROOTFS)/usr/lib/libc.a \
	$(LIBC_OBJ)

	mkdir -p $(LTOS_SYSROOT)

	cp -r $(ROOTFS)/usr \
	$(LTOS_SYSROOT)/
