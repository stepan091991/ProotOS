#ifndef PROOTOS_SYSCALL_H
#define PROOTOS_SYSCALL_H

#include "types.h"
#include "isr.h"

/* Номера syscalls (совпадают с тем, что ждёт user-space). */
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_GETPID  3
#define SYS_BRK     4

/* Стандартные файловые дескрипторы. */
#define SYS_FD_STDIN   0
#define SYS_FD_STDOUT  1
#define SYS_FD_STDERR  2

void syscall_init(void);
void syscall_handler(registers_t* regs);

#endif