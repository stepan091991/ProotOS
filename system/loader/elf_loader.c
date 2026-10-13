#include "elf_loader.h"
#include "elf.h"
#include "fat32.h"
#include "pmm.h"
#include "vmm.h"
#include "paging.h"
#include "printf.h"
#include "string.h"

elf_entry_t elf_load(const char* path) {
    fat32_file_t f;
    if (!fat32_open(path, &f)) {
        printf("[elf] cannot open %s\n", path);
        return NULL;
    }

    if (f.size < sizeof(elf32_header_t)) {
        printf("[elf] file too small: %u\n", f.size);
        fat32_close(&f);
        return NULL;
    }

    /* 1. Читаем ELF header (52 байта). */
    elf32_header_t hdr;
    if (fat32_read(&f, &hdr, sizeof(hdr)) != sizeof(hdr)) {
        printf("[elf] cannot read header\n");
        fat32_close(&f);
        return NULL;
    }

    if (hdr.e_ident[EI_MAG0] != ELF_MAGIC_0 ||
        hdr.e_ident[EI_MAG1] != ELF_MAGIC_1 ||
        hdr.e_ident[EI_MAG2] != ELF_MAGIC_2 ||
        hdr.e_ident[EI_MAG3] != ELF_MAGIC_3) {
        printf("[elf] bad magic\n");
        fat32_close(&f);
        return NULL;
    }
    if (hdr.e_ident[EI_CLASS] != ELFCLASS32) {
        printf("[elf] not 32-bit ELF\n");
        fat32_close(&f);
        return NULL;
    }
    if (hdr.e_type != ET_EXEC) {
        printf("[elf] not ET_EXEC (type=%u)\n", hdr.e_type);
        fat32_close(&f);
        return NULL;
    }
    if (hdr.e_machine != EM_386) {
        printf("[elf] not i386 (machine=%u)\n", hdr.e_machine);
        fat32_close(&f);
        return NULL;
    }

    printf("[elf] entry=%p phnum=%u\n",
           (void*) hdr.e_entry, hdr.e_phnum);

    /* 2. Обходим program headers. Каждый читаем отдельно через seek. */
    for (u16 i = 0; i < hdr.e_phnum; i++) {
        elf32_phdr_t ph;

        if (!fat32_seek(&f, hdr.e_phoff + i * hdr.e_phentsize)) break;

        if (fat32_read(&f, &ph, sizeof(ph)) != sizeof(ph)) break;

        if (ph.p_type != PT_LOAD) continue;

        printf("[elf] PT_LOAD: vaddr=%p filesz=%u memsz=%u\n",
               (void*) ph.p_vaddr, ph.p_filesz, ph.p_memsz);

        /* Мапим страницы [vstart, vend). */
        u32 vstart = ph.p_vaddr & ~0xFFF;
        u32 vend   = (ph.p_vaddr + ph.p_memsz + 0xFFF) & ~0xFFF;

        for (u32 v = vstart; v < vend; v += PAGE_SIZE) {
            u32 phys = (u32) pmm_alloc_page();
            if (!phys) {
                printf("[elf] OOM at %p\n", (void*) v);
                fat32_close(&f);
                return NULL;
            }
            vmm_map(v, phys, PAGE_RW | PAGE_USER);
        }

        /* Читаем файл с ph.p_offset, копируем в p_vaddr постранично. */
        if (!fat32_seek(&f, ph.p_offset)) {
            printf("[elf] seek failed\n");
            fat32_close(&f);
            return NULL;
        }

        u8* dst = (u8*) ph.p_vaddr;
        u32 remaining = ph.p_filesz;

        while (remaining > 0) {
            u32 chunk = remaining > 4096 ? 4096 : remaining;
            u32 got   = fat32_read(&f, dst, chunk);
            if (got == 0) {
                printf("[elf] short read\n");
                fat32_close(&f);
                return NULL;
            }
            dst       += got;
            remaining -= got;
        }

        /* Зануляем .bss (memsz > filesz). */
        if (ph.p_memsz > ph.p_filesz) {
            memset((u8*) ph.p_vaddr + ph.p_filesz, 0,
                   ph.p_memsz - ph.p_filesz);
        }
    }

    elf_entry_t entry = (elf_entry_t) hdr.e_entry;
    fat32_close(&f);
    return entry;
}

int elf_exec(const char* path) {
    elf_entry_t entry = elf_load(path);
    if (!entry) return 0;

    printf("[elf] jumping to %p\n\n", (void*) entry);
    entry();
    printf("\n[elf] program returned\n");
    return 1;
}