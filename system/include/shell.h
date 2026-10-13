#ifndef PROOTOS_SHELL_H
#define PROOTOS_SHELL_H

#include "types.h"

/* Запускает бесконечный цикл shell'а. Никогда не возвращается. */
__attribute__((noreturn))
void shell_run(void);

#endif
