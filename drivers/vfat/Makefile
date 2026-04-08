include ../../config.mk

CURRENT_DRIVER_NAME = vfat

all: ../../obj/drivers/$(CURRENT_DRIVER_NAME) ../../syspart/XT/drivers/$(CURRENT_DRIVER_NAME).xtd

SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c, ../../obj/drivers/vfat/%.o, $(SOURCES)) 

../../obj/drivers/$(CURRENT_DRIVER_NAME):
	mkdir ..\..\obj\drivers\$(CURRENT_DRIVER_NAME)

../../syspart/XT/drivers/$(CURRENT_DRIVER_NAME).xtd: $(OBJS)
	$(CC) -g $^ \
    -Wall -Wextra -Wpedantic -shared \
    -mno-red-zone -ffreestanding -nostdlib \
    -Wl,--subsystem,34 \
    -Wl,--major-subsystem-version,2 \
    -Wl,--minor-subsystem-version,0 \
	-L ../../obj/ -lxtkernel \
    -e xtDriverMain -o $@
	pexports $@ > ../../obj/drivers/$(CURRENT_DRIVER_NAME).def
	dlltool -d ../../obj/drivers/$(CURRENT_DRIVER_NAME).def -l ../../obj/drivers/$(CURRENT_DRIVER_NAME).lib

../../obj/drivers/$(CURRENT_DRIVER_NAME)/%.o:%.asm
	nasm -f win64 -o $@ $< -g

../../obj/drivers/$(CURRENT_DRIVER_NAME)/%.o:%.c
	$(CC) -c $< -o $@ -I $(UEFI_INCLUDE_DIR) \
    -I ../../include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding \
	-nostdlib