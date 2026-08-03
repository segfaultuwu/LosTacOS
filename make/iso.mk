.PHONY: iso tarfs run disk

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


	@if [ "$(BOOTLOADER)" = "limine" ]; then \
		echo "Building ISO with Limine"; \
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
    if [ "$(BOOTLOADER)" = "grub" ]; then \
      echo "Building ISO with GRUB"; \
      grub-mkrescue -o $(ISO) $(BUILD)/isodir; \
    fi \
	fi

DISK := $(BUILD)/disk0.img
DISK_SIZE := 128M


disk:
	@if [ ! -f $(DISK) ]; then \
		echo "Creating disk image $(DISK)"; \
		qemu-img create -f raw $(DISK) $(DISK_SIZE); \
	else \
		echo "Disk already exists: $(DISK)"; \
	fi

run: iso disk
	qemu-system-x86_64 \
	-cdrom $(ISO) \
	-drive id=disk0,file=$(DISK),format=raw,if=none \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk0,bus=ahci.0 \
	-serial stdio \
	-no-reboot \
	-no-shutdown
