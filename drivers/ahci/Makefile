include ../../config.mk

all: ../../obj/drivers/ahci ../../initrd/ahci.xtd

SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c, ../../obj/drivers/ahci/%.o, $(SOURCES)) 

../../obj/drivers/ahci:
	mkdir ..\..\obj\drivers\ahci

../../initrd/ahci.xtd: $(OBJS)
	$(CC) -g $^ \
    -Wall -Wextra -Wpedantic -shared \
    -mno-red-zone -ffreestanding -nostdlib \
    -Wl,--subsystem,34 \
    -Wl,--major-subsystem-version,2 \
    -Wl,--minor-subsystem-version,0 \
	-L ../../obj/ -L ../../obj/drivers/ -lxtkernel -lpci \
    -e xtDriverMain -o $@
	pexports $@ > ../../obj/drivers/ahci.def
	dlltool -d ../../obj/drivers/ahci.def -l ../../obj/drivers/ahci.lib

../../obj/drivers/ahci/%.o:%.asm
	nasm -f win64 -o $@ $< -g

../../obj/drivers/ahci/%.o:%.c
	$(CC) -c $< -o $@ -I $(UEFI_INCLUDE_DIR) \
    -I ../../include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding -O3 \
	-nostdlib