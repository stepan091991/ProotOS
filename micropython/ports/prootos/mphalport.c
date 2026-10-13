/* ports/prootos/mphalport.c */

#include "py/mpconfig.h"
#include "py/mphal.h"
#include <stddef.h>

size_t strlen(const char *s);

#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_EXIT    0

static int sys_write(int fd, const void* buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static int sys_read(int fd, void* buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

void sys_exit(int code) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(code)
        : "memory"
    );
    for (;;) { }
}

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

mp_uint_t mp_hal_ticks_ms(void) {
    return 0;
}

mp_uint_t mp_hal_ticks_us(void) {
    return 0;
}

void mp_hal_delay_us(mp_uint_t us) {
    (void)us;
}

void mp_hal_set_interrupt_char(char c) {
    (void)c;
}