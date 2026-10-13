#include "printf.h"
#include "vga.h"
#include "serial.h"

/* ─── Вывод ──────────────────────────────────────────── */

static void putc_out(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

static void puts_out(const char* s) {
    while (*s) putc_out(*s++);
}

static int str_len(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

/* ─── Числа ──────────────────────────────────────────── */

/*
 * Универсальный вывод беззнакового числа с padding.
 *   base       — 10 или 16
 *   upper      — использовать ли заглавные (для hex)
 *   width      — минимальная ширина
 *   zero_pad   — дополнять нулями (иначе пробелами)
 *   left_align — выравнивание по левому краю (флаг '-')
 */
static void print_uint_base(u32 value, u32 base, int upper,
                            int width, int zero_pad, int left_align) {
    static const char digits_lower[] = "0123456789abcdef";
    static const char digits_upper[] = "0123456789ABCDEF";
    const char* digits = upper ? digits_upper : digits_lower;

    char buf[32];
    int  n = 0;

    if (value == 0) {
        buf[n++] = '0';
    } else {
        while (value > 0 && n < 32) {
            buf[n++] = digits[value % base];
            value /= base;
        }
    }

    int pad = (width > n) ? (width - n) : 0;

    if (left_align) {
        for (int i = n - 1; i >= 0; i--) putc_out(buf[i]);
        for (int i = 0; i < pad; i++)    putc_out(' ');
        return;
    }

    char pad_char = zero_pad ? '0' : ' ';
    for (int i = 0; i < pad; i++) putc_out(pad_char);
    for (int i = n - 1; i >= 0; i--) putc_out(buf[i]);
                            }

                            /*
                             * Знаковое число.
                             * Особый случай: при zero_pad знак идёт ПЕРЕД нулями: -0042, а не 00-42.
                             */
                            static void print_int(i32 value, int width, int zero_pad, int left_align) {
                                if (value >= 0) {
                                    print_uint_base((u32) value, 10, 0, width, zero_pad, left_align);
                                    return;
                                }

                                /* Отрицательное. -(-INT_MIN) переполнит i32, поэтому abs берём в u32. */
                                u32 abs_val = (u32)(-(i64) value);

                                /* Длина abs_val. */
                                u32 tmp = abs_val;
                                int digits = 1;
                                while (tmp >= 10) { tmp /= 10; digits++; }

                                int total = digits + 1;   /* +1 за знак */
                                int pad   = (width > total) ? (width - total) : 0;

                                if (left_align) {
                                    putc_out('-');
                                    print_uint_base(abs_val, 10, 0, 0, 0, 0);
                                    for (int i = 0; i < pad; i++) putc_out(' ');
                                } else if (zero_pad) {
                                    putc_out('-');
                                    for (int i = 0; i < pad; i++) putc_out('0');
                                    print_uint_base(abs_val, 10, 0, 0, 0, 0);
                                } else {
                                    for (int i = 0; i < pad; i++) putc_out(' ');
                                    putc_out('-');
                                    print_uint_base(abs_val, 10, 0, 0, 0, 0);
                                }
                            }

                            /* ─── Основная функция ───────────────────────────────── */

                            int vprintf_impl(const char* fmt, __builtin_va_list args) {
                                int written = 0;

                                while (*fmt) {
                                    if (*fmt != '%') {
                                        putc_out(*fmt++);
                                        written++;
                                        continue;
                                    }

                                    fmt++;  /* пропускаем '%' */

                                    /* ─── Флаги ─── */
                                    int left_align = 0;
                                    int zero_pad   = 0;
                                    for (;;) {
                                        if      (*fmt == '-') { left_align = 1; fmt++; }
                                        else if (*fmt == '0') { zero_pad   = 1; fmt++; }
                                        else break;
                                    }

                                    /* ─── Ширина ─── */
                                    int width = 0;
                                    while (*fmt >= '0' && *fmt <= '9') {
                                        width = width * 10 + (*fmt - '0');
                                        fmt++;
                                    }

                                    /* ─── Спецификатор ─── */
                                    switch (*fmt) {
                                        case 's': {
                                            const char* s = __builtin_va_arg(args, const char*);
                                            if (!s) s = "(null)";

                                            int len = str_len(s);
                                            int pad = (width > len) ? (width - len) : 0;

                                            if (left_align) {
                                                puts_out(s);
                                                for (int i = 0; i < pad; i++) putc_out(' ');
                                            } else {
                                                for (int i = 0; i < pad; i++) putc_out(' ');
                                                puts_out(s);
                                            }
                                            written += len + pad;
                                            break;
                                        }
                                        case 'c': {
                                            char c = (char) __builtin_va_arg(args, int);
                                            int pad = (width > 1) ? (width - 1) : 0;
                                            if (left_align) {
                                                putc_out(c);
                                                for (int i = 0; i < pad; i++) putc_out(' ');
                                            } else {
                                                for (int i = 0; i < pad; i++) putc_out(' ');
                                                putc_out(c);
                                            }
                                            written += 1 + pad;
                                            break;
                                        }
                                        case 'd':
                                        case 'i': {
                                            i32 v = __builtin_va_arg(args, i32);
                                            print_int(v, width, zero_pad, left_align);
                                            written += (width > 0) ? width : 1;
                                            break;
                                        }
                                        case 'u': {
                                            u32 v = __builtin_va_arg(args, u32);
                                            print_uint_base(v, 10, 0, width, zero_pad, left_align);
                                            written += (width > 0) ? width : 1;
                                            break;
                                        }
                                        case 'x': {
                                            u32 v = __builtin_va_arg(args, u32);
                                            print_uint_base(v, 16, 0, width, zero_pad, left_align);
                                            written += (width > 0) ? width : 1;
                                            break;
                                        }
                                        case 'X': {
                                            u32 v = __builtin_va_arg(args, u32);
                                            print_uint_base(v, 16, 1, width, zero_pad, left_align);
                                            written += (width > 0) ? width : 1;
                                            break;
                                        }
                                        case 'p': {
                                            u32 v = (u32) __builtin_va_arg(args, void*);
                                            puts_out("0x");
                                            print_uint_base(v, 16, 0, 8, 1, 0);
                                            written += 10;
                                            break;
                                        }
                                        case '%': {
                                            putc_out('%');
                                            written++;
                                            break;
                                        }
                                        case '\0':
                                            return written;
                                        default: {
                                            putc_out('%');
                                            putc_out(*fmt);
                                            written += 2;
                                            break;
                                        }
                                    }
                                    fmt++;
                                }
                                return written;
                            }

                            int printf(const char* fmt, ...) {
                                __builtin_va_list args;
                                __builtin_va_start(args, fmt);
                                int n = vprintf_impl(fmt, args);
                                __builtin_va_end(args);
                                return n;
                            }
