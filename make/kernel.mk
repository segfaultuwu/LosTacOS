SRC_CPP := $(shell find src -name "*.cpp")
SRC_C   := $(shell find src -name "*.c")
SRC_ASM := $(shell find src -name "*.asm")


OBJ_CPP := $(patsubst src/%.cpp,$(OBJDIR)/%.cpp.o,$(SRC_CPP))
OBJ_C   := $(patsubst src/%.c,$(OBJDIR)/%.c.o,$(SRC_C))
OBJ_ASM := $(patsubst src/%.asm,$(OBJDIR)/%.asm.o,$(SRC_ASM))


OBJ = $(OBJ_CPP) $(OBJ_C) $(OBJ_ASM)

DEP = $(OBJ:.o=.d)


$(OBJDIR)/%.cpp.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@


$(OBJDIR)/%.c.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


$(OBJDIR)/%.asm.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@



$(KERNEL): version $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)
