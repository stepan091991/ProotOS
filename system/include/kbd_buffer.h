#ifndef PROOTOS_KBD_BUFFER_H
#define PROOTOS_KBD_BUFFER_H

#include "types.h"

/* Специальные клавиши, возвращаются kbd_buffer_pop() со значениями >= 0x80.
 * ASCII их не занимает, поэтому конфликта нет. */
#define KBD_KEY_UP     0x80
#define KBD_KEY_DOWN   0x81
#define KBD_KEY_LEFT   0x82
#define KBD_KEY_RIGHT  0x83
#define KBD_KEY_HOME   0x84
#define KBD_KEY_END    0x85
#define KBD_KEY_DEL    0x86

void kbd_buffer_init(void);
void kbd_buffer_push(u8 c);
int  kbd_buffer_pop(void);      /* -1 если пусто, иначе 0..255 */
int  kbd_buffer_has_data(void);
void kbd_buffer_clear(void);

#endif
