#include "types.h"
#include "vga.h"
#include "printf.h"
#include "string.h"
#include "kernel.h"
#include "panic.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "shell.h"
#include "multiboot.h"
#include "pmm.h"
#include "kheap.h"
#include "paging.h"
#include "vmm.h"
#include "ata.h"
#include "blockdev.h"
#include "ramdisk.h"
#include "fat32.h"
#include "banner.h"
#include "serial.h"
#include "tss.h"
#include "syscall.h"
#include "pci.h"
#include "process.h"

extern int  usbh_initialize(u8 busid, u32 reg_base);

/* ─── USB ──────────────────────────────────────────── */

static void usb_init(void)
{
    pci_device_t *dev = NULL;
    int count = pci_device_count();
    for (int i = 0; i < count; i++) {
        pci_device_t *d = pci_get_device(i);
        if (d->class_code == 0x0C && d->subclass == 0x03 && d->prog_if == 0x20) {
            dev = d;
            break;
        }
    }
    if (!dev) {
        printf("[usb] no EHCI controller, USB disabled\n");
        return;
    }

    printf("[usb] EHCI %04x:%04x at %02x:%02x.%x IRQ=%d\n",
           dev->vendor_id, dev->device_id,
           dev->bus, dev->slot, dev->func, dev->irq_line);

    pci_enable_mmio(dev);
    pci_enable_bus_master(dev);

    u32 bar = pci_bar_mmio(dev, 0);
    if (bar == 0) { printf("[usb] EHCI BAR0=0, aborting\n"); return; }
    printf("[usb] EHCI MMIO @ 0x%08x\n", bar);

    int ret = usbh_initialize(0, bar);
    if (ret != 0) {
        printf("[usb] usbh_initialize() failed: %d\n", ret);
        return;
    }
    printf("[usb] CherryUSB host stack ready on busid=0\n");
}

/* ─── Баннер ───────────────────────────────────────── */

static void print_banner(void) {
    print_prootos_banner();
    printf("\n ProotOS kernel v0.1.8\n");
    printf(" Booting via Multiboot / GRUB\n\n");
}

/* ─── Простые kernel-нити ──────────────────────────── */

static void idle_thread(void* arg) {
    (void) arg;
    for (;;) {
        yield();
        __asm__ volatile ("hlt");
    }
}

static void shell_thread(void* arg) {
    (void) arg;
    shell_run();
}

/* ─── Boot monitor ─────────────────────────────────── */
//
// Ждёт появления usb0 в таблице block_device'ов, монтирует её как
// корневой FAT32-том. Если usb0 не появился за 15 секунд —
// фоллбэк на ata0, потом на initrd.
//
// И только ПОСЛЕ успешного монтирования создаёт shell-нить.

#define BOOT_TIMEOUT_TICKS  1500   /* 15 сек при PIT=100 Гц */

static void boot_monitor_thread(void* arg)
{
    (void) arg;

    block_device_t* chosen = NULL;
    const char*     chosen_name = NULL;

    // ─── 1. Ждём usb0 до 15 секунд ───
    printf("[boot] waiting for USB storage (usb0)...\n");
    u64 t0 = pit_get_ticks();
    while ((pit_get_ticks() - t0) < BOOT_TIMEOUT_TICKS) {
        block_device_t* usb0 = bdev_get("usb0");
        if (usb0 && usb0->total_sectors > 0) {
            chosen = usb0;
            chosen_name = "usb0";
            break;
        }
        process_sleep(100);
    }

    // ─── 2. Монтируем в порядке приоритета ───
    if (chosen && fat32_mount(chosen)) {
        printf("[boot] mounted %s as root filesystem\n", chosen_name);
        fat32_test();
    } else {
        chosen = NULL;

        // Fallback: ata0
        block_device_t* ata0 = bdev_get("ata0");
        if (ata0 && ata0->total_sectors > 0 && fat32_mount(ata0)) {
            chosen = ata0;
            chosen_name = "ata0";
            printf("[boot] USB unavailable, mounted %s as root\n", chosen_name);
            fat32_test();
        }
    }

    if (!chosen) {
        // Fallback: initrd
        block_device_t* rd = bdev_get("initrd");
        if (rd && fat32_mount(rd)) {
            chosen = rd;
            chosen_name = "initrd";
            printf("[boot] USB/ATA unavailable, mounted %s as root\n", chosen_name);
            fat32_test();
        }
    }

    if (!chosen) {
        printf("[boot] WARNING: no mountable filesystem\n");
    } else {
        printf("[boot] root filesystem: %s\n", chosen_name);
    }

    // ─── 3. Готово. Запускаем shell ───
    printf("[boot] starting shell...\n");
    process_create("shell", shell_thread, NULL);

    // ─── 4. Boot monitor завершил работу. Уходим спать навсегда. ───
    for (;;) {
        process_sleep(10000);
    }
}

/* ─── kmain ────────────────────────────────────────── */

void kmain(u32 multiboot_info_addr) {
    serial_init();
    serial_write("\n\n=== ProotOS booting ===\n");

    vga_init();
    print_banner();

    vga_set_color(VGA_YELLOW, VGA_BLACK);
    printf("[OK] VGA driver initialized\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    multiboot_info_t* mbi = (multiboot_info_t*) multiboot_info_addr;

    pmm_init(mbi);

    if ((mbi->flags & MULTIBOOT_INFO_MODS) && mbi->mods_count > 0) {
        multiboot_module_t* mods = (multiboot_module_t*) mbi->mods_addr;
        for (u32 i = 0; i < mbi->mods_count; i++) {
            pmm_mark_region_used(mods[i].mod_start, mods[i].mod_end);
        }
    }

    paging_init();
    kheap_init();

    gdt_init();
    tss_init();
    idt_init();
    syscall_init();
    pic_init();
    pit_init(100);
    keyboard_init();

    __asm__ volatile ("sti");
    printf("[OK] interrupts enabled\n");

    ata_init();
    pci_init();
    pci_dump();

    printf("[boot] initializing process table...\n");
    process_init();

    // ─── 1. idle создаётся первым ───
    process_create("idle", idle_thread, NULL);

    // ─── 2. Регистрируем initrd (для фоллбэка в boot_monitor) ───
    if ((mbi->flags & MULTIBOOT_INFO_MODS) && mbi->mods_count > 0) {
        multiboot_module_t* mods = (multiboot_module_t*) mbi->mods_addr;
        u32 start = mods[0].mod_start;
        u32 end   = mods[0].mod_end;
        block_device_t* rd = ramdisk_create("initrd", start, end - start);
        if (rd) printf("[boot] ramdisk 'initrd' registered\n");
    }

    // ─── 3. USB ───
    printf("[boot] initializing USB (creates usbh_hub0)...\n");
    usb_init();

    // ─── 4. Boot monitor: ждёт usb0, монтирует, запускает shell ───
    process_create("boot", boot_monitor_thread, NULL);

    bdev_dump();
    process_dump();

    printf("[boot] starting scheduler...\n");
    schedule();

    panic("schedule returned!");
}
