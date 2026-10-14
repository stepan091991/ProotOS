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
#define SYS_UPTIME  8
#define SYS_STAT    9

/* flags для SYS_OPEN */
#define O_RDONLY    0x0
#define O_WRONLY    0x1
#define O_CREAT     0x2
#define O_TRUNC     0x4

/* возврат SYS_STAT */
#define STAT_NOENT  0
#define STAT_FILE   1
#define STAT_DIR    2

void syscall_init(void);
void syscall_handler(registers_t* regs);

#endif
