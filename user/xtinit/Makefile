include ../../config.mk

all: ../../obj/xtkernel/arch/$(ARCH) ../../initrd/xtinit.xte


SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c, ../../obj/user/xtinit/%.o, $(SOURCES)) 

../../obj/user/xtinit/arch/$(ARCH):
	mkdir ..\..\obj\user\xtinit\arch\$(ARCH)

../../obj/user/xtinit:
	mkdir ..\..\obj\user\xtinit

../../initrd/xtinit.xte: $(OBJS)
	$(CC) -g $^ \
    -Wall -Wextra -Wpedantic \
    -mno-red-zone -ffreestanding -nostdlib \
    -Wl,--subsystem,32 \
    -Wl,--major-subsystem-version,2 \
    -Wl,--minor-subsystem-version,0 \
    -e xtMain -o $@

../../obj/user/xtinit/%.o:%.asm
	nasm -f win64 -o $@ $< -g

../../obj/user/xtinit/%.o:%.c
	$(CC) -c $< -o $@ -I $(UEFI_INCLUDE_DIR) \
    -I ../../include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding -O3 \
	-nostdlib