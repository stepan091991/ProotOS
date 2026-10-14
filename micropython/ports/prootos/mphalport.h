#ifndef MICROPY_INCLUDED_PROOTOS_MPHALPORT_H
#define MICROPY_INCLUDED_PROOTOS_MPHALPORT_H

#include "py/mpconfig.h"

int  mp_hal_stdin_rx_chr(void);
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len);
void mp_hal_stdout_tx_str(const char *str);
void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len);
void mp_hal_set_interrupt_char(char c);
mp_uint_t mp_hal_ticks_ms(void);
mp_uint_t mp_hal_ticks_us(void);
mp_uint_t mp_hal_ticks_cpu(void);
void mp_hal_delay_us(mp_uint_t us);
void mp_hal_delay_ms(mp_uint_t ms);

/* Сисколы. */
int  sys_open(const char* path, int flags);
int  sys_close(int fd);
int  sys_read(int fd, void* buf, unsigned int len);
int  sys_write(int fd, const void* buf, unsigned int len);
int  sys_stat(const char* path);
int  sys_uptime(void);
int  sys_sleep(int ms);

#endif
