#ifndef PROOTOS_SYSCALL_H
#define PROOTOS_SYSCALL_H

#include "types.h"
#include "isr.h"

#define SYS_EXIT   0
#define SYS_WRITE  1

void syscall_init(void);
void syscall_handler(registers_t* regs);

#endif
