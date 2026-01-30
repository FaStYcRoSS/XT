
include ../config.mk

ifeq ($(ARCH), x86_64)
BOOTSTUB_NAME=BOOTX64
endif

all: ../root/EFI/BOOT/$(BOOTSTUB_NAME).EFI

../root/EFI/BOOT/$(BOOTSTUB_NAME).EFI: ../root/EFI/XT/XTBOOT.EFI
	copy E:\Projects\XT\root\EFI\XT\XTBOOT.EFI E:\Projects\XT\root\EFI\BOOT\$(BOOTSTUB_NAME).EFI /B /Y