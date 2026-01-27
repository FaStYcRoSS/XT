
MODULES = xtboot bootstub xtkernel user libs

.PHONY: all $(MODULES) image clean run

all: image

$(MODULES):
	$(MAKE) -C $@

image: $(MODULES)
	python tools/mkimage.py

run: image
	qemu-system-x86_64 \
	-D logs/log.txt -d in_asm -monitor stdio \
	-bios ./build/ovmf-code-x86_64.fd \
	-usb \
	-no-reboot -no-shutdown \
	-drive format=raw,unit=0,file=./build/xtos.img \
	-serial file:logs/serial.txt \
	-device usb-mouse \
	-rtc base=localtime \
	-m 256M \
	-machine q35
