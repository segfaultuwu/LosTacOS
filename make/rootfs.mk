.PHONY: rootfs rootfs_dirs headers


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


rootfs: rootfs_dirs headers libc init
	cp -r $(ROOTDIR)/* $(ROOTFS)/
