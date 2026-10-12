#include "elf_loader.h"
#include "elf.h"
#include "fat32.h"
#include "pmm.h"
#include "vmm.h"
#include "paging.h"
#include "printf.h"
#include "string.h"
#include "kheap.h"
#include "gdt.h"
#include "usermode.h"

elf_entry_t elf_load(const char* path) {
    fat32_file_t f;
    if (!fat32_open(path, &f)) {
        printf("[elf] cannot open %s\n", path);
        return NULL;
    }

    if (f.size < sizeof(elf32_header_t)) {
        printf("[elf] file too small\n");
        fat32_close(&f);
        return NULL;
    }

    u8* buf = (u8*) kmalloc(f.size);
    if (!buf) {
        printf("[elf] OOM\n");
        fat32_close(&f);
        return NULL;
    }

    u32 total = 0;
    while (total < f.size) {
        u32 n = fat32_read(&f, buf + total, f.size - total);
        if (n == 0) break;
        total += n;
    }
    fat32_close(&f);

    if (total != f.size) {
        printf("[elf] short read\n");
        kfree(buf);
        return NULL;
    }

    elf32_header_t* hdr = (elf32_header_t*) buf;

    if (hdr->e_ident[EI_MAG0] != ELF_MAGIC_0 ||
        hdr->e_ident[EI_MAG1] != ELF_MAGIC_1 ||
        hdr->e_ident[EI_MAG2] != ELF_MAGIC_2 ||
        hdr->e_ident[EI_MAG3] != ELF_MAGIC_3 ||
        hdr->e_ident[EI_CLASS] != ELFCLASS32 ||
        hdr->e_type != ET_EXEC ||
        hdr->e_machine != EM_386) {
        printf("[elf] not a valid i386 ET_EXEC\n");
    kfree(buf);
    return NULL;
        }

        printf("[elf] entry=%p phnum=%u\n",
               (void*) hdr->e_entry, hdr->e_phnum);

        for (u16 i = 0; i < hdr->e_phnum; i++) {
            elf32_phdr_t* ph = (elf32_phdr_t*)
            (buf + hdr->e_phoff + i * hdr->e_phentsize);

            if (ph->p_type != PT_LOAD) continue;

            printf("[elf] PT_LOAD: vaddr=%p filesz=%u memsz=%u\n",
                   (void*) ph->p_vaddr, ph->p_filesz, ph->p_memsz);

            u32 start = ph->p_vaddr & ~0xFFF;
            u32 end   = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFF;

            for (u32 v = start; v < end; v += PAGE_SIZE) {
                u32 phys = (u32) pmm_alloc_page();
                if (!phys) {
                    printf("[elf] OOM\n");
                    kfree(buf);
                    return NULL;
                }
                vmm_map(v, phys, PAGE_RW | PAGE_USER);  /* ← PAGE_USER */
            }

            u8* dst = (u8*) ph->p_vaddr;
            memcpy(dst, buf + ph->p_offset, ph->p_filesz);
            if (ph->p_memsz > ph->p_filesz) {
                memset(dst + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
            }
        }

        elf_entry_t entry = (elf_entry_t) hdr->e_entry;
        kfree(buf);
        return entry;
}

int elf_exec(const char* path) {
    elf_entry_t entry = elf_load(path);
    if (!entry) return 0;

    /* Аллоцируем user stack — одну страницу (4 КБ). */
    u32 stack_phys = (u32) pmm_alloc_page();
    if (!stack_phys) {
        printf("[elf] cannot allocate user stack\n");
        return 0;
    }

    /* Кладём user stack по адресу 0xB0000000 (высоко, далеко от кода). */
    u32 user_stack_top = 0xB0000000 + PAGE_SIZE;
    vmm_map(0xB0000000, stack_phys, PAGE_RW | PAGE_USER);

    printf("[elf] entering user mode: entry=%p  stack_top=%p\n",
           (void*) entry, (void*) user_stack_top);

    /* Никогда не вернётся через обычный путь — только через syscall exit. */
    enter_user_mode((u32) entry, user_stack_top);

    printf("[elf] program returned (via syscall exit)\n");
    return 1;
}
