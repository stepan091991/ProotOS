#ifndef PROOTOS_MULTIBOOT_H
#define PROOTOS_MULTIBOOT_H

#include "types.h"

#define MULTIBOOT_INFO_MEMORY   (1 << 0)
#define MULTIBOOT_INFO_BOOTDEV  (1 << 1)
#define MULTIBOOT_INFO_CMDLINE  (1 << 2)
#define MULTIBOOT_INFO_MODS     (1 << 3)
#define MULTIBOOT_INFO_MMAP     (1 << 6)

#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    u32 size;
    u64 base_addr;
    u64 length;
    u32 type;
} __attribute__((packed)) multiboot_mmap_entry_t;

typedef struct {
    u32 mod_start;
    u32 mod_end;
    u32 cmdline;
    u32 pad;
} __attribute__((packed)) multiboot_module_t;

#endif
