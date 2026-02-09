include ../config.mk

all: ../obj/xtkernel/arch/$(ARCH) ../root/XT/xtkernel.exe

ASM_SOURCES = $(wildcard arch/$(ARCH)/*.asm)
SOURCES = $(wildcard *.c) $(wildcard arch/$(ARCH)/*.c) 
OBJS = $(patsubst %.c, ../obj/xtkernel/%.o, $(SOURCES)) ../obj/external/printf.o  $(patsubst %.asm, ../obj/xtkernel/%.o, $(ASM_SOURCES))

../obj/xtkernel/arch/$(ARCH):
	mkdir ..\obj\xtkernel\arch\$(ARCH)

../root/XT/xtkernel.exe: $(OBJS)
	$(CC) -g $^ \
    -Wall -Wextra -Wpedantic \
    -mno-red-zone -ffreestanding -O3 -nostdlib \
    -Wl,--subsystem,10 \
    -Wl,--major-subsystem-version,2 \
    -Wl,--minor-subsystem-version,0 \
    -Wl,--image-base,0xffffffff80000000 \
    -e xtKernelMain -o $@

../obj/xtkernel/%.o:%.asm
	nasm -f win64 -o $@ $< -g

../obj/xtkernel/%.o:%.c
	$(CC) -c $< -o $@ -I $(UEFI_INCLUDE_DIR) \
    -I ../include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding -O3 \
	-nostdlib