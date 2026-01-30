include ../config.mk

all: ../root/XT/xtkernel.exe

SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c, ../obj/xtkernel/%.o, $(SOURCES)) ../obj/external/printf.o 

../root/XT/xtkernel.exe: $(OBJS)
	$(CC) -g $^ \
    -Wall -Wextra -Wpedantic \
    -mno-red-zone -ffreestanding -O3 -nostdlib \
    -Wl,--subsystem,10 \
    -Wl,--major-subsystem-version,2 \
    -Wl,--minor-subsystem-version,0 \
    -Wl,--image-base,0xffffffff80000000 \
    -e xtKernelMain -o $@

../obj/xtkernel/%.o:%.c
	$(CC) -c $< -o $@ -I $(UEFI_INCLUDE_DIR) \
    -I ../include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding -O3 \
	-nostdlib