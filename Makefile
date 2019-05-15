CC=$(N64_INST)/bin/mips64-elf-gcc
AS=$(N64_INST)/bin/mips64-elf-as
LD=$(N64_INST)/bin/mips64-elf-ld
OBJCOPY = $(N64_INST)/bin/mips64-elf-objcopy
N64TOOL = $(N64_INST)/bin/n64tool
CHKSUM64PATH = $(N64_INST)/bin/chksum64
HEADERPATH = $(N64_INST)/mips64-elf/lib/header
#ABI = -mabi=64 -msym32
ABI = -mabi=o64
COMMONFLAGS = -march=vr4300 -mtune=vr4300 -G0 $(ABI)
CFLAGS = $(COMMONFLAGS) -std=c90 -ffunction-sections -fdata-sections -O0 -ggdb3 -Wall -Werror
ASFLAGS = $(COMMONFLAGS) -g

STUBOBJS = gdbstub.o gdbstubl.o

BASENAME = sample
ROM = $(BASENAME).z64
BIN = $(BASENAME).bin
ELF = $(BASENAME).elf
OBJS = sample.o $(STUBOBJS)
LIBS = -ldragon -lc -ldragonsys

.PHONY: all clean

all: $(ROM)

clean:
	-rm $(ROM) $(BIN) $(ELF) $(OBJS)

$(ROM): $(BIN)
	rm -f $@
	$(N64TOOL) -l 1028K -t "some program" -h $(HEADERPATH) -o $@ $<
	$(CHKSUM64PATH) $@

$(BIN): $(ELF)
	$(OBJCOPY) $< $@ -O binary

$(ELF): $(OBJS)
	$(CC) -Tn64ld.x -Wl,-Map,$@.map -o $@ $^ $(LIBS)

gdbstubl.o: gdbstubl.S gdbstub.h
gdbstub.o: gdbstub.c gdbstub.h
