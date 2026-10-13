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

static void print_banner(void) {
    print_prootos_banner();
    printf("\n ProotOS kernel v0.1.8\n");
    printf(" Booting via Multiboot / GRUB\n\n");
}

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

/* Счётчики для теста — видны через `threads`. */
volatile u32 g_work1_count = 0;
volatile u32 g_work2_count = 0;

static void work1_thread(void* arg) {
    (void) arg;
    for (;;) {
        g_work1_count++;
        process_sleep(500);   /* раз в 500 мс */
    }
}

static void work2_thread(void* arg) {
    (void) arg;
    for (;;) {
        g_work2_count++;
        process_sleep(700);   /* раз в 700 мс */
    }
}

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


    block_device_t* mounted = bdev_get("usb0");
    if (!mounted) {
        block_device_t* ata0 = bdev_get("ata0");
        if (ata0 && fat32_mount(ata0)) {
            mounted = ata0;
            printf("[boot] working disk: %s (ATA)\n", ata0->name);
            fat32_test();
        }
    }

    if (!mounted &&
        (mbi->flags & MULTIBOOT_INFO_MODS) && mbi->mods_count > 0) {
        multiboot_module_t* mods = (multiboot_module_t*) mbi->mods_addr;
        u32 start = mods[0].mod_start;
        u32 end   = mods[0].mod_end;
        block_device_t* rd = ramdisk_create("initrd", start, end - start);
        if (rd && fat32_mount(rd)) {
            mounted = rd;
            printf("[boot] working disk: initrd (RAM)\n");
            fat32_test();
        }
    }

    if (!mounted) printf("[boot] WARNING: no mountable filesystem\n");

    bdev_dump();

    printf("[boot] initializing processes...\n");
    process_init();
    process_create("idle",  idle_thread,  NULL);
    process_create("shell", shell_thread, NULL);
    process_create("work1", work1_thread, NULL);
    process_create("work2", work2_thread, NULL);
    process_dump();

    printf("[boot] starting scheduler...\n");
    schedule();

    panic("schedule returned!");
}
