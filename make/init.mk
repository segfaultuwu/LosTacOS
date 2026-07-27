.PHONY: init

INIT_SRC = bin/init.c
INIT_OBJ = $(BUILD)/init.o
INIT_BIN = $(ROOTFS)/bin/init


$(INIT_OBJ): $(INIT_SRC)
	@mkdir -p $(dir $@)

	$(LTOSCC) \
		-c $< \
		-o $@



$(INIT_BIN): $(INIT_OBJ)

	@mkdir -p $(dir $@)

	$(LTOSLD) \
		$@ \
		$(INIT_OBJ)



init: $(INIT_BIN)
