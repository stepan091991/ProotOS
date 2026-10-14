/* ports/prootos/mphalport.c */

#include "py/mpconfig.h"
#include "py/mphal.h"
#include <stddef.h>

size_t strlen(const char *s);

/* ─── Сырой syscall через int 0x80 ─────────────────
 * Сохраняем ebx/esi/edi явно — GCC с -O2 их использует,
 * и без push/pop наши аргументы затираются. */

int sys3(int nr, int a, int b, int c) {
    int ret;
    __asm__ volatile (
        "int  $0x80"
        : "=a"(ret)
        : "a"(nr), "b"(a), "c"(b), "d"(c)
        : "memory"
    );
    return ret;
}

int sys_write(int fd, const void* buf, unsigned int len) {
    return sys3(1, fd, (int)(size_t) buf, (int) len);
}

int sys_read(int fd, void* buf, unsigned int len) {
    return sys3(2, fd, (int)(size_t) buf, (int) len);
}

void sys_exit(int code) {
    sys3(0, code, 0, 0);
    for (;;) { }
}

/* ─── HAL MicroPython ────────────────────────────── */

int mp_hal_stdin_rx_chr(void) {
    unsigned char c = 0;
    while (sys_read(0, &c, 1) <= 0) { }
    if (c == '\n') return '\r';
    return c;
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    sys_write(1, str, len);
    return len;
}

void mp_hal_stdout_tx_str(const char *str) {
    mp_hal_stdout_tx_strn(str, strlen(str));
}

void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
    mp_hal_stdout_tx_strn(str, len);
}

mp_uint_t mp_hal_ticks_ms(void)  { return 0; }
mp_uint_t mp_hal_ticks_us(void)  { return 0; }
void      mp_hal_delay_us(mp_uint_t us) { (void)us; }
void      mp_hal_set_interrupt_char(char c) { (void)c; }
