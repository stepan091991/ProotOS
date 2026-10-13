#ifndef PROOTOS_USERMODE_H
#define PROOTOS_USERMODE_H

#include "types.h"

/* Перейти в ring 3. Не возвращается напрямую — только через
 * syscall_exit, который восстанавливает сохранённый esp ядра. */
void enter_user_mode(u32 entry, u32 user_stack);

/* Вызывается из syscall_exit — восстанавливает esp и делает ret,
 * возвращая управление в elf_exec. */
void usermode_return_to_kernel(void);

#endif
