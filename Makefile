
MODULES = xtboot bootstub xtkernel user libs drivers

.PHONY: all $(MODULES) image clean run

all: logs obj efipart syspart image initrd

logs:
	mkdir $@

obj:
	mkdir $@

efipart:
	mkdir $@

syspart:
	mkdir $@

initrd:
	mkdir $@

$(MODULES):
	$(MAKE) -C $@

./build/xtos.img: $(MODULES)
	tar -c initrd > efipart/XT/xtinitrd
	python tools/mkimage.py

run: ./build/xtos.img
	qemu-system-x86_64 \
	-D logs/log.txt -d in_asm,int -monitor stdio \
	-bios ./build/bios64.fd \
	-usb \
	-no-reboot -no-shutdown \
	-drive format=raw,unit=0,file=./build/xtos.img \
	-serial file:logs/serial.txt \
	-device usb-mouse \
	-rtc base=localtime \
	-m 256M \
	-machine q35
