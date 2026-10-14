/* ports/prootos/mphalport.c */

#include "py/mpconfig.h"
#include "py/mphal.h"
#include <stddef.h>

size_t strlen(const char *s);

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_OPEN    3
#define SYS_CLOSE   4

/* Сырой syscall через int 0x80.
 * Регистры фиксируем constraint'ами — GCC сам разложит по eax/ebx/ecx/edx. */
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
    return sys3(SYS_WRITE, fd, (int)(size_t) buf, (int) len);
}

int sys_read(int fd, void* buf, unsigned int len) {
    return sys3(SYS_READ, fd, (int)(size_t) buf, (int) len);
}

int sys_open(const char* path, int flags) {
    return sys3(SYS_OPEN, (int)(size_t) path, flags, 0);
}

int sys_close(int fd) {
    return sys3(SYS_CLOSE, fd, 0, 0);
}

void sys_exit(int code) {
    sys3(SYS_EXIT, code, 0, 0);
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
