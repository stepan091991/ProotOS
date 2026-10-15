#ifndef PROOTOS_COMPAT_H
#define PROOTOS_COMPAT_H

/* Подтягивается через -include во все .c при сборке порта.
 * Нужно, чтобы lib/ (crypto-algorithms, re1.5, uzlib) и другие
 * файлы, рассчитывающие на "нормальный" хост, видели stdint /
 * stdbool / string / size_t и MP_FALLTHROUGH. */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* lib/re1.5 использует MP_FALLTHROUGH БЕЗ собственной ';',
 * поэтому макрос обязан включать её в себя. Так же определяет
 * py/mpconfig.h — переопределение будет безвредным (одинаковое). */
#ifndef MP_FALLTHROUGH
#define MP_FALLTHROUGH __attribute__((fallthrough));
#endif

#endif
