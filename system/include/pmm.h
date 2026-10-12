#ifndef PROOTOS_PMM_H
#define PROOTOS_PMM_H

#include "types.h"
#include "multiboot.h"

#define PMM_PAGE_SIZE  4096
#define PMM_PAGE_SHIFT 12

void pmm_init(multiboot_info_t* mbi);
void* pmm_alloc_page(void);
void  pmm_free_page(void* page);
u32 pmm_get_free_pages(void);
u32 pmm_get_total_pages(void);
void pmm_dump(void);

/* Пометить регион [start, end) как занятый. */
void pmm_mark_region_used(u32 start, u32 end);

#endif
