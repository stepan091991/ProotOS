#ifndef PROOTOS_SERIAL_H
#define PROOTOS_SERIAL_H

#include "types.h"

#define SERIAL_COM1 0x3F8

/* Инициализация COM1 на 38400 бод, 8N1 */
void serial_init(void);

/* Записать один символ. Игнорируется, если serial не инициализирован. */
void serial_putchar(char c);

/* Записать строку */
void serial_write(const char* str);

/* Готов ли serial принимать данные */
int  serial_is_initialized(void);

#endif
