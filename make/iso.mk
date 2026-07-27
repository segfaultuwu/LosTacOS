.PHONY: iso tarfs run


tarfs: rootfs

	$(TAR) \
	--format=ustar \
	-cf $(TARFS) \
	-C $(ROOTFS) .



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



run: iso

	qemu-system-x86_64 \
	-cdrom $(ISO) \
	-serial stdio \
	-no-reboot \
	-no-shutdown
