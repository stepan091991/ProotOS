#ifndef PROOTOS_PRINTF_H
#define PROOTOS_PRINTF_H

#include "types.h"

/*
 * Минимальный printf.
 * Поддерживает: %s %c %d %i %u %x %X %p %%
 * Ширина и выравнивание: %5d, %-5d, %05d, %14s, %-14s, %08x
 */
int printf(const char* fmt, ...);

int vprintf_impl(const char* fmt, __builtin_va_list args);

#endif
