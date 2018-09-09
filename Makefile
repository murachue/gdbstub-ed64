CC=$(N64_INST)/bin/mips64-elf-gcc
AS=$(N64_INST)/bin/mips64-elf-as
LD=$(N64_INST)/bin/mips64-elf-ld
OBJCOPY = $(N64_INST)/bin/mips64-elf-objcopy
N64TOOL = $(N64_INST)/bin/n64tool
CHKSUM64PATH = $(N64_INST)/bin/chksum64
HEADERPATH = $(N64_INST)/mips64-elf/lib/header
#ABI = -mabi=64 -msym32
ABI = -mabi=o64
CFLAGS = -std=c90 -ffunction-sections -fdata-sections -march=vr4300 -mtune=vr4300 $(ABI) -O0 -G0 -ggdb3 -Wall -Werror
ASFLAGS =  -march=vr4300 -mtune=vr4300 -g $(ABI)

STUBOBJS = gdbstub.o gdbstubl.o cache.o

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

gdbstubl.o: gdbstubl.S 3264.h gdbstub.h
gdbstub.o: gdbstub.c 3264.h regs.h cache.h gdbstub.h
cache.o: cache.c cache.h
