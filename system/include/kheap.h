#ifndef PROOTOS_KHEAP_H
#define PROOTOS_KHEAP_H

#include "types.h"

/* Инициализация: резервирует первый блок страниц у PMM. */
void  kheap_init(void);

/* Аллокация произвольного размера. Возвращает NULL, если нет памяти. */
void* kmalloc(size_t size);

/* Аллокация с обнулением. */
void* kcalloc(size_t count, size_t size);

/* Освобождение. Безопасно для NULL. */
void  kfree(void* ptr);

/* Статистика — пригодится для команды 'heap' в shell. */
void  kheap_dump(void);

#endif
