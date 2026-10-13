#ifndef PROOTOS_TSS_H
#define PROOTOS_TSS_H

#include "types.h"

#define GDT_TSS_SELECTOR  0x28

void tss_init(void);

/* Установить стек ядра, который будет использоваться при переходе ring3→ring0. */
void tss_set_kernel_stack(u32 esp0);

#endif
