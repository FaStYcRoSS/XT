include ../config.mk

all: mkimage.exe

SOURCES = $(wildcard *.c) 
OBJS = $(patsubst %.c, ../obj/mkimage/%.o, $(SOURCES))  \
	../obj/xtkernel/fs.o ../obj/xtkernel/list.o ../obj/xtkernel/string.o ../obj/xtkernel/gpt.o \
	../obj/drivers/vfat/main.o ../obj/xtkernel/time.o ../obj/xtkernel/encodings.o


mkimage.exe: $(OBJS)
	$(CC) -g $^ \
	-fvisibility=hidden \
    -Wall -Wextra -Wpedantic \
    -o $@

../obj/mkimage/%.o:%.asm
	nasm -f win64 -o $@ $< -g

../obj/mkimage/%.o:%.c
	$(CC) -c $< -o $@ -I $(UEFI_INCLUDE_DIR) \
    -I ../include -Wall -Wextra -g \
	-Wpedantic -mno-red-zone \
	-ffreestanding -O3 \
	-nostdlib