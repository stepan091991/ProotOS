#include "kbd_buffer.h"

#define KBD_BUFFER_SIZE 128

static volatile u8  buffer[KBD_BUFFER_SIZE];
static volatile u32 head = 0;
static volatile u32 tail = 0;

void kbd_buffer_init(void) {
    head = 0;
    tail = 0;
}

void kbd_buffer_push(u8 c) {
    u32 next = (head + 1) & (KBD_BUFFER_SIZE - 1);
    if (next == tail) return;
    buffer[head] = c;
    head = next;
}

int kbd_buffer_pop(void) {
    if (head == tail) return -1;
    u8 c = buffer[tail];
    tail = (tail + 1) & (KBD_BUFFER_SIZE - 1);
    return (int) c;
}

int kbd_buffer_has_data(void) {
    return head != tail;
}

void kbd_buffer_clear(void) {
    head = 0;
    tail = 0;
}
