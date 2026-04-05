
MODULES = xtboot bootstub xtkernel user libs drivers tools

.PHONY: all $(MODULES) ./build/xtos.img clean run

all: logs obj efipart syspart ./build/xtos.img initrd

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
	tools/mkimage

compile: ./build/xtos.img

run:
	qemu-system-x86_64 \
	-D logs/log.txt -d in_asm -D logs/log.txt -monitor stdio \
	-bios ./build/bios64.fd \
	-usb \
	-drive format=raw,unit=0,file=./build/xtos.img \
	-serial file:logs/serial.txt \
	-device usb-mouse \
	-rtc base=localtime \
	-m 256M \
	-machine q35
