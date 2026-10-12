CC = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -nostdlib -fno-pic -fno-pie \
         -fno-stack-protector -O2 -Wall -Wextra

LDFLAGS = -m elf_i386 -z noseparate-code -T hello/linker.ld

all: hello.elf

hello.o: hello/hello.c
	$(CC) $(CFLAGS) -c $< -o $@

hello.elf: hello.o hello/linker.ld
	$(LD) $(LDFLAGS) -o $@ hello.o

clean:
	rm -f *.o *.elf

.PHONY: all clean
