.PHONY: iso tarfs run


tarfs: rootfs

	$(TAR) \
	--format=ustar \
	-cf $(BUILD)/rootfs.tar \
	-C $(ROOTFS) .
	gzip -f $(BUILD)/rootfs.tar



iso: $(KERNEL) tarfs

	rm -rf $(BUILD)/isodir

	mkdir -p \
	$(BUILD)/isodir/boot/grub \
	$(BUILD)/isodir/EFI/BOOT


	cp $(KERNEL) \
	$(BUILD)/isodir/boot/kernel.elf


	cp $(TARFS) \
	$(BUILD)/isodir/boot/rootfs.tar.gz


	cp assets/font.psf.gz \
	$(BUILD)/isodir/boot/font.psf.gz


	cp cfg/grub.cfg \
	$(BUILD)/isodir/boot/grub/grub.cfg


	cp cfg/limine.conf \
	$(BUILD)/isodir/boot/limine.conf


	cp cfg/limine.conf \
	$(BUILD)/isodir/limine.conf


	@if [ "$(BOOTLOADER)" = "limine" ] && command -v limine >/dev/null 2>&1 && [ -f /usr/share/limine/limine-bios-cd.bin ]; then \
		echo "Building ISO with default bootloader: Limine"; \
		cp /usr/share/limine/limine-bios-cd.bin $(BUILD)/isodir/boot/; \
		cp /usr/share/limine/limine-bios.sys $(BUILD)/isodir/boot/; \
		if [ -f /usr/share/limine/limine-uefi-cd.bin ]; then \
			cp /usr/share/limine/limine-uefi-cd.bin $(BUILD)/isodir/boot/; \
		fi; \
		if [ -f /usr/share/limine/BOOTX64.EFI ]; then \
			cp /usr/share/limine/BOOTX64.EFI $(BUILD)/isodir/EFI/BOOT/; \
		fi; \
		if [ -f /usr/share/limine/limine-uefi-cd.bin ]; then \
			xorriso -as mkisofs -b boot/limine-bios-cd.bin \
				-no-emul-boot -boot-load-size 4 -boot-info-table \
				--efi-boot boot/limine-uefi-cd.bin \
				-efi-boot-part --efi-boot-image --protective-msdos-label \
				$(BUILD)/isodir -o $(ISO); \
		else \
			xorriso -as mkisofs -b boot/limine-bios-cd.bin \
				-no-emul-boot -boot-load-size 4 -boot-info-table \
				$(BUILD)/isodir -o $(ISO); \
		fi; \
		limine bios-install $(ISO); \
	else \
		echo "Limine not found. Falling back to GRUB bootloader"; \
		grub-mkrescue -o $(ISO) $(BUILD)/isodir; \
	fi



run: iso

	qemu-system-x86_64 \
	-cdrom $(ISO) \
	-serial stdio \
	-no-reboot \
	-no-shutdown
