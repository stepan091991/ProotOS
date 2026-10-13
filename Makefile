NAME     = prootos
ARCH     = i386

ASM      = nasm
CC       = gcc
LD       = ld
GRUB     = grub-mkrescue
QEMU     = qemu-system-$(ARCH)

ASMFLAGS = -f elf32
CFLAGS   = -m32 -ffreestanding -O2 -Wall -Wextra \
           -fno-stack-protector -fno-pic \
           -I system/include -I system/drivers -I system/utils \
           -I system/drivers/vga/
LDFLAGS  = -m elf_i386 -T system/kernel/linker.ld

BUILD_DIR = build
ISO_DIR   = iso

ASM_SOURCES = $(shell find system -name '*.S')
C_SOURCES   = $(shell find system -name '*.c')

ASM_OBJECTS = $(patsubst system/%.S, $(BUILD_DIR)/%.o, $(ASM_SOURCES))
C_OBJECTS   = $(patsubst system/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
OBJECTS     = $(ASM_OBJECTS) $(C_OBJECTS)

KERNEL_ELF = $(BUILD_DIR)/$(NAME).elf
ISO_IMAGE  = $(BUILD_DIR)/$(NAME).iso

# ─── Образы ──────────────────────────────────────────
INITRD_IMG   = $(BUILD_DIR)/data.img
INITRD_SIZE  = 64
INITRD_SRC   = $(wildcard initrd_files/*)

DISK_IMG     = $(BUILD_DIR)/disk.img
DISK_SIZE    = 128

USB_IMG      = $(BUILD_DIR)/usb.img
USB_SIZE     = 64

USER_PROG    = programs/hello.elf

.PHONY: all clean run debug programs usb

all: $(ISO_IMAGE) $(DISK_IMG) $(USB_IMG)

# ─── Компиляция ядра ─────────────────────────────────
$(BUILD_DIR)/%.o: system/%.S
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.o: system/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(OBJECTS) system/kernel/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# ─── Пользовательская программа ──────────────────────
programs: $(USER_PROG)

$(USER_PROG):
	$(MAKE) -C programs

# ─── initrd (RAM disk) ───────────────────────────────
$(INITRD_IMG): $(INITRD_SRC)
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(INITRD_IMG) bs=1M count=$(INITRD_SIZE) 2>/dev/null
	mkfs.fat -F 32 -s 1 -n PROOTOS $(INITRD_IMG) >/dev/null 2>&1
	@if [ -d initrd_files ]; then \
	    for f in initrd_files/*; do \
	        [ -f "$$f" ] || continue; \
	        name=$$(basename "$$f" | tr a-z A-Z); \
	        mcopy -o -i $(INITRD_IMG) "$$f" "::$$name"; \
	    done; \
	fi

# ─── persistent ATA disk ─────────────────────────────
$(DISK_IMG): $(INITRD_SRC) $(USER_PROG)
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=$(DISK_SIZE) 2>/dev/null
	mkfs.fat -F 32 -s 1 -n PROOTOS $(DISK_IMG) >/dev/null 2>&1
	@if [ -d initrd_files ]; then \
	    for f in initrd_files/*; do \
	        [ -f "$$f" ] || continue; \
	        name=$$(basename "$$f" | tr a-z A-Z); \
	        mcopy -o -i $(DISK_IMG) "$$f" "::$$name"; \
	    done; \
	fi
	@if [ -f $(USER_PROG) ]; then \
	    mcopy -o -i $(DISK_IMG) $(USER_PROG) ::HELLO.ELF; \
	    echo "[disk] HELLO.ELF installed"; \
	fi

# ─── USB flash (эмулируется в QEMU) ──────────────────
$(USB_IMG):
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(USB_IMG) bs=1M count=$(USB_SIZE) 2>/dev/null
	mkfs.fat -F 32 -s 1 -n USBFLASH $(USB_IMG) >/dev/null 2>&1
	@if [ -d initrd_files ]; then \
	    for f in initrd_files/*; do \
	        [ -f "$$f" ] || continue; \
	        name=$$(basename "$$f" | tr a-z A-Z); \
	        mcopy -o -i $(USB_IMG) "$$f" "::$$name"; \
	    done; \
	fi
	@echo "[usb] $(USB_IMG) built ($(USB_SIZE) MB)"

usb: $(USB_IMG)

# ─── ISO ─────────────────────────────────────────────
$(ISO_IMAGE): $(KERNEL_ELF) $(INITRD_IMG)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/$(NAME).elf
	cp $(INITRD_IMG) $(ISO_DIR)/boot/data.img
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	$(GRUB) -o $(ISO_IMAGE) $(ISO_DIR)

# ─── Запуск в QEMU ───────────────────────────────────
run: $(ISO_IMAGE) $(DISK_IMG) $(USB_IMG)
	$(QEMU) -m 512 -cdrom $(ISO_IMAGE) -boot d \
	    -drive file=$(DISK_IMG),format=raw,if=ide,index=0,media=disk \
	    -device piix3-usb-uhci,id=uhci \
	    -drive if=none,id=usbdisk,file=$(USB_IMG),format=raw \
	    -device usb-storage,drive=usbdisk,bus=uhci.0 \
	    -serial file:serial.log \
	    -monitor stdio

debug: $(ISO_IMAGE) $(DISK_IMG) $(USB_IMG)
	$(QEMU) -m 512 -cdrom $(ISO_IMAGE) -boot d \
	    -drive file=$(DISK_IMG),format=raw,if=ide,index=0,media=disk \
	    -device piix3-usb-uhci,id=uhci \
	    -drive if=none,id=usbdisk,file=$(USB_IMG),format=raw \
	    -device usb-storage,drive=usbdisk,bus=uhci.0 \
	    -s -S -serial file:serial.log &
	gdb -ex "target remote localhost:1234" \
	    -ex "symbol-file $(KERNEL_ELF)"

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR)
	@if [ -d programs ]; then $(MAKE) -C programs clean; fi
