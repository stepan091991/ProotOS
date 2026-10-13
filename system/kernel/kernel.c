#include "kernel.h"

void halt(void) {
    /* Запрещаем прерывания и останавливаем CPU навсегда. */
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
