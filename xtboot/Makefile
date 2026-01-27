include ../config.mk

all: $(PROJECT_DIR)/root/EFI/XT/XTBOOT.EFI

SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c, $(OBJ_DIR)/xtboot/%.o, $(SOURCES))

$(PROJECT_DIR)/root/EFI/XT/XTBOOT.EFI: $(OBJS)
	$(CC) -g $^ \
    -Wall -Wextra -Wpedantic \
    -mno-red-zone -ffreestanding -O3 -nostdlib \
    -Wl,--subsystem,10 \
    -Wl,--major-subsystem-version,2 \
    -Wl,--minor-subsystem-version,0 \
    -e UefiMain -o $@

$(OBJ_DIR)/xtboot/%.o:%.c
	$(CC) -c $< -o $@ -I D:/UEFI/ \
    -I $(PROJECT_DIR)/include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding -O3 \
	-nostdlib