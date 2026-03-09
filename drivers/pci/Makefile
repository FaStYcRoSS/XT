include ../../config.mk

all: ../../obj/drivers/pci ../../initrd/pci.xtd

SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c, ../../obj/drivers/pci/%.o, $(SOURCES)) 

../../obj/drivers/pci:
	mkdir ..\..\obj\drivers\pci

../../initrd/pci.xtd: $(OBJS)
	$(CC) -g $^ \
    -Wall -Wextra -Wpedantic -shared \
    -mno-red-zone -ffreestanding -nostdlib \
    -Wl,--subsystem,34 \
    -Wl,--major-subsystem-version,2 \
    -Wl,--minor-subsystem-version,0 \
	-L ../../obj/ -lxtkernel \
    -e xtDriverMain -o $@
	pexports $@ > ../../obj/drivers/pci.def
	dlltool -d ../../obj/drivers/pci.def -l ../../obj/drivers/pci.lib

../../obj/drivers/pci/%.o:%.asm
	nasm -f win64 -o $@ $< -g

../../obj/drivers/pci/%.o:%.c
	$(CC) -c $< -o $@ -I $(UEFI_INCLUDE_DIR) \
    -I ../../include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding -O3 \
	-nostdlib