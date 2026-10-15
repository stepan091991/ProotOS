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
           -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
           -I system/include -I system/drivers -I system/utils \
           -I system/drivers/vga/ \
           -I system/drivers/usb \
           -I thirdparty/CherryUSB \
           -I thirdparty/CherryUSB/common \
           -I thirdparty/CherryUSB/core \
           -I thirdparty/CherryUSB/class/hub \
           -I thirdparty/CherryUSB/class/hid \
           -I thirdparty/CherryUSB/class/msc \
           -I thirdparty/CherryUSB/class/cdc \
           -I thirdparty/CherryUSB/port/ehci \
           -include system/drivers/usb/usb_config.h
LDFLAGS  = -m elf_i386 -T system/kernel/linker.ld

BUILD_DIR = build
ISO_DIR   = iso

ASM_SOURCES = $(shell find system -name '*.S')
C_SOURCES   = $(shell find system -name '*.c')

ASM_OBJECTS = $(patsubst system/%.S, $(BUILD_DIR)/%.o, $(ASM_SOURCES))
C_OBJECTS   = $(patsubst system/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
OBJECTS     = $(ASM_OBJECTS) $(C_OBJECTS)

# ─── CherryUSB ───
CHERRYUSB_DIR = thirdparty/CherryUSB
CHERRYUSB_SOURCES = \
    $(CHERRYUSB_DIR)/core/usbh_core.c \
    $(CHERRYUSB_DIR)/class/hub/usbh_hub.c \
    $(CHERRYUSB_DIR)/port/ehci/usb_hc_ehci.c \
    $(CHERRYUSB_DIR)/class/hid/usbh_hid.c \
    $(CHERRYUSB_DIR)/class/msc/usbh_msc.c

CHERRYUSB_OBJECTS = $(patsubst $(CHERRYUSB_DIR)/%.c, $(BUILD_DIR)/cherryusb/%.o, $(CHERRYUSB_SOURCES))

# ─── MicroPython ───
MPY_PORT     = micropython/ports/prootos
MPY_FIRMWARE = $(MPY_PORT)/build/firmware.elf

KERNEL_ELF = $(BUILD_DIR)/$(NAME).elf
ISO_IMAGE  = $(BUILD_DIR)/$(NAME).iso

# ─── Образы ───
INITRD_IMG   = $(BUILD_DIR)/data.img
INITRD_SIZE  = 64
INITRD_SRC   = $(shell find initrd_files 2>/dev/null)

DISK_IMG     = $(BUILD_DIR)/disk.img
DISK_SIZE    = 128

USB_IMG      = $(BUILD_DIR)/usb.img
USB_SIZE     = 64

USER_PROG    = programs/hello.elf
BIN_DIR      = initrd_files/BIN

MPY_DEST     = $(BIN_DIR)/MPYTHON.ELF

.PHONY: all clean run debug programs usb flash flash-iso flash-disk flash-list bin-all \
        micropython mpython

# all: сборка идёт в правильном порядке. bin-all и micropython кладут
# свежие артефакты в initrd_files/BIN/, потом строятся образы.
all: bin-all micropython $(ISO_IMAGE) $(DISK_IMG) $(USB_IMG)

# ─── Компиляция ядра ───
$(BUILD_DIR)/%.o: system/%.S
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.o: system/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cherryusb/%.o: $(CHERRYUSB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(OBJECTS) $(CHERRYUSB_OBJECTS) system/kernel/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(CHERRYUSB_OBJECTS)

# ─── Пользовательские программы ───
programs: $(USER_PROG)

$(USER_PROG):
	$(MAKE) -C programs

bin-all:
	@$(MAKE) --no-print-directory -C programs 2>/dev/null || true
	@mkdir -p $(BIN_DIR)
	@for p in programs/*.elf; do \
	    [ -f "$$p" ] || continue; \
	    base=$$(basename "$$p" .elf); \
	    up=$$(echo "$$base" | tr a-z A-Z); \
	    cp -f "$$p" "$(BIN_DIR)/$$up.ELF"; \
	    echo "[bin] $$p → $(BIN_DIR)/$$up.ELF"; \
	done

# ─── MicroPython ───
micropython: $(MPY_DEST)

$(MPY_DEST):
	@echo "[mpy] building micropython port..."
	@$(MAKE) -C $(MPY_PORT) 2>&1 | tee $(MPY_PORT)/build.log | tail -n 10
	@if [ ! -f $(MPY_FIRMWARE) ]; then \
	    echo "!!! ERROR: $(MPY_FIRMWARE) не собран."; \
	    echo "!!! Смотри $(MPY_PORT)/build.log"; \
	    exit 1; \
	fi
	@mkdir -p $(BIN_DIR)
	@cp -f $(MPY_FIRMWARE) $@
	@echo "[mpy] $(MPY_FIRMWARE) → $@"
	@ls -lh $@

mpython:
	@$(MAKE) -C $(MPY_PORT) clean >/dev/null 2>&1 || true
	@rm -f $(MPY_DEST)
	@$(MAKE) micropython

# ─── initrd (RAM disk) ───
# Зависит от USER_PROG и MPY_DEST — иначе может собраться раньше,
# чем bin-all/micropython положат свежие ELF'ы в initrd_files/BIN/.
$(INITRD_IMG): $(INITRD_SRC) $(MPY_DEST) $(USER_PROG)
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(INITRD_IMG) bs=1M count=$(INITRD_SIZE) 2>/dev/null
	mkfs.fat -F 32 -s 1 -n PROOTOS $(INITRD_IMG) >/dev/null 2>&1
	@if [ -d initrd_files ]; then \
	    find initrd_files -mindepth 1 -type d \
	        | awk -F/ '{print NF-1, $$0}' | sort -n | cut -d' ' -f2- \
	        | while read d; do \
	            U=$$(echo "$${d#initrd_files/}" | tr a-z A-Z); \
	            mmd -i $(INITRD_IMG) "::/$$U" 2>/dev/null || true; \
	        done; \
	    find initrd_files -type f | while read f; do \
	        rel=$${f#initrd_files/}; \
	        dir=$$(dirname "$$rel"); \
	        base=$$(basename "$$rel"); \
	        if [ "$$dir" = "." ]; then \
	            tgt=$$(echo "$$base" | tr a-z A-Z); \
	        else \
	            tgt=$$(echo "$$dir" | tr a-z A-Z)/$$(echo "$$base" | tr a-z A-Z); \
	        fi; \
	        mcopy -o -i $(INITRD_IMG) "$$f" "::/$$tgt"; \
	    done; \
	fi

# ─── persistent ATA disk ───
$(DISK_IMG): $(INITRD_SRC) $(MPY_DEST) $(USER_PROG)
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=$(DISK_SIZE) 2>/dev/null
	mkfs.fat -F 32 -s 1 -n PROOTOS $(DISK_IMG) >/dev/null 2>&1
	@if [ -d initrd_files ]; then \
	    find initrd_files -mindepth 1 -type d \
	        | awk -F/ '{print NF-1, $$0}' | sort -n | cut -d' ' -f2- \
	        | while read d; do \
	            U=$$(echo "$${d#initrd_files/}" | tr a-z A-Z); \
	            mmd -i $(DISK_IMG) "::/$$U" 2>/dev/null || true; \
	        done; \
	    find initrd_files -type f | while read f; do \
	        rel=$${f#initrd_files/}; \
	        dir=$$(dirname "$$rel"); \
	        base=$$(basename "$$rel"); \
	        if [ "$$dir" = "." ]; then \
	            tgt=$$(echo "$$base" | tr a-z A-Z); \
	        else \
	            tgt=$$(echo "$$dir" | tr a-z A-Z)/$$(echo "$$base" | tr a-z A-Z); \
	        fi; \
	        mcopy -o -i $(DISK_IMG) "$$f" "::/$$tgt"; \
	    done; \
	fi
	@if [ -f $(USER_PROG) ]; then \
	    mcopy -o -i $(DISK_IMG) $(USER_PROG) ::/HELLO.ELF; \
	    echo "[disk] HELLO.ELF installed"; \
	fi

# ─── USB flash (эмулируется в QEMU) ───
# Собирается из initrd_files. Зависит от USER_PROG и MPY_DEST,
# чтобы пересобираться при обновлении артефактов.
$(USB_IMG): $(INITRD_SRC) $(MPY_DEST) $(USER_PROG)
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(USB_IMG) bs=1M count=$(USB_SIZE) 2>/dev/null
	mkfs.fat -F 32 -s 1 -n USBFLASH $(USB_IMG) >/dev/null 2>&1
	@if [ -d initrd_files ]; then \
	    find initrd_files -mindepth 1 -type d \
	        | awk -F/ '{print NF-1, $$0}' | sort -n | cut -d' ' -f2- \
	        | while read d; do \
	            U=$$(echo "$${d#initrd_files/}" | tr a-z A-Z); \
	            mmd -i $(USB_IMG) "::/$$U" 2>/dev/null || true; \
	        done; \
	    find initrd_files -type f | while read f; do \
	        rel=$${f#initrd_files/}; \
	        dir=$$(dirname "$$rel"); \
	        base=$$(basename "$$rel"); \
	        if [ "$$dir" = "." ]; then \
	            tgt=$$(echo "$$base" | tr a-z A-Z); \
	        else \
	            tgt=$$(echo "$$dir" | tr a-z A-Z)/$$(echo "$$base" | tr a-z A-Z); \
	        fi; \
	        mcopy -o -i $(USB_IMG) "$$f" "::/$$tgt"; \
	    done; \
	fi
	@echo "[usb] $(USB_IMG) built ($(USB_SIZE) MB)"

usb: $(USB_IMG)

# ─── ISO ───
$(ISO_IMAGE): $(KERNEL_ELF) $(INITRD_IMG)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/$(NAME).elf
	cp $(INITRD_IMG) $(ISO_DIR)/boot/data.img
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	$(GRUB) -o $(ISO_IMAGE) $(ISO_DIR)

# ─── Запуск в QEMU ───
run: $(ISO_IMAGE) $(DISK_IMG) $(USB_IMG)
	$(QEMU) -m 512 -cdrom $(ISO_IMAGE) -boot d \
	    -drive file=$(DISK_IMG),format=raw,if=ide,index=0,media=disk \
	    -device usb-ehci,id=ehci \
	    -device usb-kbd,bus=ehci.0 \
	    -drive if=none,id=usbdisk,file=$(USB_IMG),format=raw \
	    -device usb-storage,drive=usbdisk,bus=ehci.0 \
	    -serial file:serial.log \
	    -monitor stdio

debug: $(ISO_IMAGE) $(DISK_IMG) $(USB_IMG)
	$(QEMU) -m 512 -cdrom $(ISO_IMAGE) -boot d \
	    -drive file=$(DISK_IMG),format=raw,if=ide,index=0,media=disk \
	    -device usb-ehci,id=ehci \
	    -device usb-kbd,bus=ehci.0 \
	    -drive if=none,id=usbdisk,file=$(USB_IMG),format=raw \
	    -device usb-storage,drive=usbdisk,bus=ehci.0 \
	    -s -S -serial file:serial.log &
	gdb -ex "target remote localhost:1234" \
	    -ex "symbol-file $(KERNEL_ELF)"

# ─── Прошивка ───
.PHONY: flash flash-iso flash-disk flash-list

flash-list:
	@echo "Доступные блочные устройства:"
	@lsblk -o NAME,SIZE,TYPE,MOUNTPOINT,MODEL 2>/dev/null || true
	@echo ""
	@echo "Использование: make flash DEV=/dev/sdX"

define check_dev
	@if [ -z "$(DEV)" ]; then \
	    echo "ОШИБКА: не указано устройство."; \
	    echo "Использование: make flash DEV=/dev/sdX"; \
	    exit 1; \
	fi
	@if [ ! -b "$(DEV)" ]; then \
	    echo "ОШИБКА: $(DEV) не является блочным устройством."; \
	    exit 1; \
	fi
	@if lsblk -no MOUNTPOINT $(DEV) 2>/dev/null | grep -q .; then \
	    echo "ВНИМАНИЕ: $(DEV) смонтирован. Размонтируй: sudo umount $(DEV)*"; \
	    exit 1; \
	fi
endef

flash-iso: $(ISO_IMAGE)
	$(call check_dev)
	@echo "=================================================="
	@echo "  Устройство: $(DEV)"
	@lsblk -o NAME,SIZE,TYPE,MODEL $(DEV) 2>/dev/null || true
	@echo "  Образ:      $(ISO_IMAGE) ($$(stat -c%s $(ISO_IMAGE) | numfmt --to=iec))"
	@echo ""
	@echo "  Содержимое initrd (data.img):"
	@mdir -i $(INITRD_IMG) -/ :: 2>/dev/null || echo "    (mdir не установлен)"
	@echo ""
	@echo "  ВСЁ СОДЕРЖИМОЕ $(DEV) БУДЕТ УНИЧТОЖЕНО."
	@echo "=================================================="
	@read -p "Продолжить? [y/N] " ans; \
	if [ "$$ans" != "y" ] && [ "$$ans" != "Y" ]; then \
	    echo "Отменено."; exit 1; \
	fi
	-sudo umount $(DEV)* 2>/dev/null || true
	@echo "[flash] пишем $(ISO_IMAGE) на $(DEV)..."
	sudo dd if=$(ISO_IMAGE) of=$(DEV) bs=4M status=progress conv=fsync
	sudo sync
	@echo "[flash] готово."

flash: flash-iso

flash-disk: $(DISK_IMG)
	$(call check_dev)
	@echo "=================================================="
	@echo "  Устройство: $(DEV)"
	@lsblk -o NAME,SIZE,TYPE,MODEL $(DEV) 2>/dev/null || true
	@echo "  Образ:      $(DISK_IMG) ($$(stat -c%s $(DISK_IMG) | numfmt --to=iec))"
	@echo ""
	@echo "  Содержимое (disk.img):"
	@mdir -i $(DISK_IMG) -/ :: 2>/dev/null || echo "    (mdir не установлен)"
	@echo ""
	@echo "  ВСЁ СОДЕРЖИМОЕ $(DEV) БУДЕТ УНИЧТОЖЕНО."
	@echo "=================================================="
	@read -p "Продолжить? [y/N] " ans; \
	if [ "$$ans" != "y" ] && [ "$$ans" != "Y" ]; then \
	    echo "Отменено."; exit 1; \
	fi
	-sudo umount $(DEV)* 2>/dev/null || true
	@echo "[flash] пишем $(DISK_IMG) на $(DEV)..."
	sudo dd if=$(DISK_IMG) of=$(DEV) bs=4M status=progress conv=fsync
	sudo sync
	@echo "[flash] готово."

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR)
	@if [ -d programs ]; then $(MAKE) -C programs clean; fi
	@$(MAKE) -C $(MPY_PORT) clean >/dev/null 2>&1 || true
	@rm -f $(MPY_DEST)
