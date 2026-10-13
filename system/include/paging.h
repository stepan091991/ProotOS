#ifndef PROOTOS_PAGING_H
#define PROOTOS_PAGING_H

#include "types.h"

#define PAGE_SIZE 4096

/* Биты в PTE / PDE */
#define PAGE_PRESENT   0x001
#define PAGE_RW        0x002
#define PAGE_USER      0x004
#define PAGE_ACCESSED  0x020
#define PAGE_DIRTY     0x040
#define PAGE_ADDR_MASK 0xFFFFF000

/* Инициализация: identity-map 4 ГБ, включить paging. */
void paging_init(void);

/* Проверка состояния */
int  paging_is_enabled(void);

/* Диагностика CR0/CR3 */
void paging_dump(void);

void paging_test_unmap(void);

#endif
