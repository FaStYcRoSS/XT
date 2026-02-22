
include ../config.mk

ifeq ($(ARCH), x86_64)
BOOTSTUB_NAME=BOOTX64
endif

all: ../efipart/EFI/BOOT/$(BOOTSTUB_NAME).EFI

../efipart/EFI/BOOT/$(BOOTSTUB_NAME).EFI: ../efipart/EFI/XT/XTBOOT.EFI
	copy E:\Projects\XT\efipart\EFI\XT\XTBOOT.EFI E:\Projects\XT\efipart\EFI\BOOT\$(BOOTSTUB_NAME).EFI /B /Y