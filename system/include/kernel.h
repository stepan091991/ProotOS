#ifndef PROOTOS_KERNEL_H
#define PROOTOS_KERNEL_H

#include "types.h"

#define PROOTOS_VERSION_MAJOR 0
#define PROOTOS_VERSION_MINOR 0
#define PROOTOS_VERSION_PATCH 3

/* Бесконечная остановка процессора. Никогда не возвращает управление. */
__attribute__((noreturn))
void halt(void);

#endif
