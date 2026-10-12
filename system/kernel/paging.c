#include "paging.h"
#include "pmm.h"
#include "printf.h"
#include "string.h"
#include "panic.h"

#define ENTRIES_PER_TABLE  1024   /* записей в одной таблице */
#define TABLES_PER_DIR     1024   /* таблиц в одной директории */

typedef u32 pde_t;   /* page directory entry */
typedef u32 pte_t;   /* page table entry */

static pde_t* page_directory  = NULL;
static int    paging_enabled  = 0;

/* ─── Включение paging ───────────────────────────────── */

void paging_init(void) {
    /* 1. Аллоцируем Page Directory — одну страницу.
     *    Она автоматически выровнена на 4 КБ (PMM так делает). */
    page_directory = (pde_t*) pmm_alloc_page();
    if (!page_directory) {
        panic("paging: cannot allocate page directory");
    }
    memset(page_directory, 0, PAGE_SIZE);

    /* 2. Для каждой из 1024 записей PD аллоцируем Page Table
     *    и identity-map'им соответствующий регион 4 МБ. */
    for (u32 i = 0; i < TABLES_PER_DIR; i++) {
        pte_t* pt = (pte_t*) pmm_alloc_page();
        if (!pt) {
            panic("paging: cannot allocate page table");
        }

        for (u32 j = 0; j < ENTRIES_PER_TABLE; j++) {
            /* Адрес, который мапит эта запись:
             *   i-я PD-запись покрывает 4 МБ, начиная с (i << 22)
             *   j-я PT-запись покрывает 4 КБ, начиная с (j << 12)
             */
            u32 phys = (i << 22) | (j << 12);
            pt[j] = phys | PAGE_PRESENT | PAGE_RW;
        }

        page_directory[i] = ((u32) pt) | PAGE_PRESENT | PAGE_RW;
    }

    /* 3. Загружаем адрес PD в CR3.
     *    С этого момента процессор ЗНАЕТ про таблицы,
     *    но ещё их не использует, потому что PG = 0. */
    __asm__ volatile ("mov %0, %%cr3" : : "r"((u32) page_directory));

    /* 4. Включаем paging: устанавливаем бит 31 (PG) в CR0.
     *    Следующая инструкция будет FETCH'иться через таблицы.
     *    Если identity-map корректна — мы продолжим работу
     *    с того же места, как ни в чём не бывало. */
    u32 cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    paging_enabled = 1;
    printf("[paging] enabled, PD at %p, %u page tables (%u MB mapped)\n",
           page_directory, TABLES_PER_DIR, TABLES_PER_DIR * 4);
}

int paging_is_enabled(void) {
    return paging_enabled;
}

void paging_dump(void) {
    u32 cr0, cr2, cr3;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    printf("[paging] CR0 = %p  (PG=%u, PE=%u)\n",
           (void*) cr0, (cr0 >> 31) & 1, cr0 & 1);
    printf("[paging] CR3 = %p  (physical address of PD)\n", (void*) cr3);
    printf("[paging] CR2 = %p  (last page-fault address)\n", (void*) cr2);
}

void paging_test_unmap(void) {
    u32 eip;
    __asm__ volatile (
        "lea 1f, %0\n\t"
        "1:\n\t"
        : "=r"(eip)
    );

    u32 pd_idx = eip >> 22;
    u32 pt_idx = (eip >> 12) & 0x3FF;

    u32 pd_phys;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(pd_phys));
    u32* pd = (u32*) pd_phys;
    u32* pt = (u32*)(pd[pd_idx] & PAGE_ADDR_MASK);

    pt[pt_idx] &= ~PAGE_PRESENT;
    __asm__ volatile ("invlpg (%0)" : : "r"(eip & ~0xFFF));

    printf("[paging] unmapped page at %p\n", (void*)(eip & ~0xFFF));
}
