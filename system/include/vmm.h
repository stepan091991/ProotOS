#ifndef PROOTOS_VMM_H
#define PROOTOS_VMM_H

#include "types.h"

/* Граница между kernel- и user-частью адресного пространства.
 * Всё, что ниже — kernel (identity map, общее).
 * Всё, что выше — user (у каждого процесса своё). */
#define VMM_KERNEL_LIMIT  0x40000000   /* 1 ГБ */
#define VMM_KERNEL_PD_ENTRIES  (VMM_KERNEL_LIMIT >> 22)   /* = 256 */

/* Получить физический адрес текущего PD (из CR3). */
u32 vmm_get_current_pd(void);

/* Переключиться на другое адресное пространство (загрузить CR3). */
void vmm_switch_address_space(u32 pd_phys);

/* Создать новое адресное пространство.
 * Копирует kernel-часть PD (указатели на общие PT),
 * user-часть пустая. Возвращает физический адрес нового PD
 * или 0 при ошибке. */
u32 vmm_create_address_space(void);

/* Замапить страницу: virt -> phys. Флаги — PAGE_* из paging.h. */
void vmm_map(u32 virt, u32 phys, u32 flags);

/* Снять маппинг. */
void vmm_unmap(u32 virt);

/* Получить физический адрес, соответствующий virt.
 * Возвращает 0, если страница не замаплена. */
u32 vmm_get_physical(u32 virt);

/* Проверить, замаплена ли страница. */
int vmm_is_mapped(u32 virt);

/* Диагностика: печатает цепочку PD → PT → PTE для адреса. */
void vmm_dump(u32 virt);

/* Тест: замапить/размапить/проверить. Вызывается из shell. */
void vmm_test(void);

#endif
