#ifndef PROOTOS_PANIC_H
#define PROOTOS_PANIC_H

#include "types.h"

/* Безусловная остановка ядра с сообщением. */
__attribute__((noreturn))
void panic(const char* message);

/* То же, но с указанием файла и строки. */
__attribute__((noreturn))
void panic_at(const char* file, int line, const char* message);

/* ASSERT: если условие ложно — паника с указанием места. */
#define ASSERT(cond, msg)                       \
do {                                        \
    if (!(cond)) {                          \
        panic_at(__FILE__, __LINE__, msg);  \
    }                                       \
} while (0)

#endif
