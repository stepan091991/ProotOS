#ifndef PROOTOS_GDT_H
#define PROOTOS_GDT_H

#include "types.h"

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

/* Инициализация GDT и загрузка в процессор. */
void gdt_init(void);

/* Заполнить произвольную запись GDT (используется tss.c). */
void gdt_set_entry(int index, u32 base, u32 limit,
                   u8 access, int gran_4k, int is_32bit);

#endif
