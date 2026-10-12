#ifndef PROOTOS_PIT_H
#define PROOTOS_PIT_H

#include "types.h"

/* Частота, которую мы будем генерировать (Гц) */
#define PIT_FREQUENCY 100

/* Инициализация PIT */
void pit_init(u32 frequency);

/* Сколько тиков прошло с момента старта */
u64 pit_get_ticks(void);

#endif
