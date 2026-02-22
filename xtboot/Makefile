include ../config.mk

all: ../efipart/EFI/XT/XTBOOT.EFI

ASM_SOURCES = $(wildcard arch/$(ARCH)/*.asm)
SOURCES = $(wildcard *.c) $(wildcard arch/$(ARCH)/*.c) 
OBJS = $(patsubst %.c, ../obj/xtboot/%.o, $(SOURCES)) ../obj/external/printf.o \
    $(patsubst %.asm, ../obj/xtboot/%.o, $(ASM_SOURCES))

../efipart/EFI/XT/XTBOOT.EFI: $(OBJS)
	$(CC) -g $^ \
    -Wall -Wextra -Wpedantic \
    -mno-red-zone -ffreestanding -O3 -nostdlib \
    -Wl,--subsystem,10 \
    -Wl,--major-subsystem-version,2 \
    -Wl,--minor-subsystem-version,0 \
    -e UefiMain -o $@

../obj/xtboot/%.o:%.asm
	nasm -f win64 -o $@ $< -g

../obj/xtboot/%.o:%.c
	$(CC) -c $< -o $@ -I D:/UEFI/ \
    -I ../include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding -O3 \
	-nostdlib