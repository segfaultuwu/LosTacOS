.PHONY: toolchain


toolchain: headers

	cp cfg/user.ld \
	$(HOME)/.ltos/linker.ld

	cp tools/ltos* \
	$(HOME)/.local/bin/

	chmod +x \
	$(HOME)/.local/bin/ltos*

	rm -r $(HOME)/.ltos/toolchain/sysroot/usr/include

	mkdir -p $(HOME)/.ltos/toolchain/sysroot/usr/
	mkdir -p $(HOME)/.ltos/toolchain/sysroot/lib/

	cp -r libc/include $(HOME)/.ltos/toolchain/sysroot/usr/include
	cp build/libc/src/crt0.o $(HOME)/.ltos/toolchain/sysroot/lib/
