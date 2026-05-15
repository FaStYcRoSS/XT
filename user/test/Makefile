include ../../config.mk

all: ../../obj/xtkernel/arch/$(ARCH) ../../initrd/test.xte 


SOURCES = $(wildcard *.c)
ASM_SOURCES = $(wildcard *.asm)
OBJS = $(patsubst %.c, ../../obj/user/test/%.o, $(SOURCES)) ../../obj/external/printf.o \
		$(patsubst %.asm, ../../obj/user/test/%.o, $(ASM_SOURCES))

../../obj/user/test/arch/$(ARCH):
	mkdir ..\..\obj\user\test\arch\$(ARCH)

../../obj/user/test:
	mkdir ..\..\obj\user\test

../../initrd/test.xte: $(OBJS)
	$(CC) -g $^ \
    -Wall -Wextra -Wpedantic \
    -mno-red-zone -ffreestanding -nostdlib \
    -Wl,--subsystem,32 \
    -e xtMain -o $@

../../obj/user/test/%.o:%.asm
	nasm -f win64 -o $@ $< -g

../../obj/user/test/%.o:%.c
	$(CC) -c $< -o $@ -I $(UEFI_INCLUDE_DIR) \
    -I ../../include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding -O3 \
	-nostdlib