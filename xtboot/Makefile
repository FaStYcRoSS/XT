include ../config.mk

all: ../root/EFI/XT/XTBOOT.EFI

SOURCES = $(wildcard *.c)
ASM_SOURCES = $(wildcard *.asm)
OBJS = $(patsubst %.c, ../obj/xtboot/%.o, $(SOURCES)) $(patsubst %.asm, ../obj/xtboot/%.o, $(ASM_SOURCES)) ../obj/external/printf.o 

../root/EFI/XT/XTBOOT.EFI: $(OBJS)
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