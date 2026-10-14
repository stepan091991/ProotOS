#ifndef PROOTOS_SYSCALL_H
#define PROOTOS_SYSCALL_H

#include "types.h"
#include "isr.h"

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_OPEN    3
#define SYS_CLOSE   4
#define SYS_LSEEK   5
#define SYS_GETPID  6
#define SYS_BRK     7

void syscall_init(void);
void syscall_handler(registers_t* regs);

#endif
