#include "string.h"

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') len++;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(u8)*a - (int)(u8)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(u8)*a - (int)(u8)*b;
}

char* strcpy(char* dst, const char* src) {
    char* ret = dst;
    while ((*dst++ = *src++) != '\0') { }
    return ret;
}

char* strncpy(char* dst, const char* src, size_t n) {
    char* ret = dst;
    while (n && *src) { *dst++ = *src++; n--; }
    while (n--) *dst++ = '\0';
    return ret;
}

char* strcat(char* dst, const char* src) {
    char* ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++) != '\0') { }
    return ret;
}

void* memset(void* ptr, int c, size_t n) {
    u8* p = (u8*) ptr;
    u8  v = (u8) c;
    while (n--) *p++ = v;
    return ptr;
}

void* memcpy(void* dst, const void* src, size_t n) {
    u8*       d = (u8*) dst;
    const u8* s = (const u8*) src;
    while (n--) *d++ = *s++;
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const u8* pa = (const u8*) a;
    const u8* pb = (const u8*) b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

void* memchr(const void* ptr, int c, size_t n) {
    const u8* p = (const u8*) ptr;
    u8        v = (u8) c;
    while (n--) {
        if (*p == v) return (void*) p;
        p++;
    }
    return NULL;
}

void strrev(char* str) {
    size_t len = strlen(str);
    if (len < 2) return;
    size_t i = 0;
    size_t j = len - 1;
    while (i < j) {
        char tmp  = str[i];
        str[i]    = str[j];
        str[j]    = tmp;
        i++; j--;
    }
}

/* ─── Дополнение для CherryUSB ─── */

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if ((char)c == '\0') return (char *)s;
    return NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if ((char)c == '\0') return (char *)s;
    return (char *)last;
}

/* Минимальный strtol. Поддерживает: [пробелы] [+/-] [0x]цифры.
 * База: 0 = автоопределение (0x = 16, 0 = 8, иначе 10). */
long strtol(const char *s, char **endptr, int base)
{
    const char *p = s;
    long result = 0;
    int neg = 0;

    while (*p == ' ' || *p == '\t' || *p == '\n') p++;

    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') { p++; }

    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16; p += 2;
        } else if (p[0] == '0') {
            base = 8; p++;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    }

    const char *start = p;
    while (*p) {
        int d;
        if      (*p >= '0' && *p <= '9') d = *p - '0';
        else if (*p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') d = *p - 'A' + 10;
        else break;
        if (d >= base) break;
        result = result * base + d;
        p++;
    }

    if (endptr) *endptr = (char *)(p == start ? s : p);
    return neg ? -result : result;
}

/* ─────────────────────────────────────────────────────────
 * snprintf — минимальная реализация для CherryUSB.
 * Поддерживает: %s %c %d %i %u %x %X %p %% и
 *               ширину (%5d, %-5d, %05d, %14s, %-14s, %08x)
 * Не поддерживает: точность (.3f), long long, float.
 * ───────────────────────────────────────────────────────── */

typedef __builtin_va_list va_list_t;
#define va_start_t(ap, last) __builtin_va_start(ap, last)
#define va_arg_t(ap, type)   __builtin_va_arg(ap, type)
#define va_end_t(ap)         __builtin_va_end(ap)

struct snp_out {
    char  *buf;
    size_t cap;      // полный размер буфера, включая место под '\0'
    size_t pos;      // сколько уже записано (без учёта '\0')
};

static void snp_putc(struct snp_out *o, char c)
{
    if (o->pos + 1 < o->cap) {
        o->buf[o->pos] = c;
    }
    o->pos++;
}

static void snp_puts(struct snp_out *o, const char *s, int width, int left)
{
    int len = 0;
    const char *p = s;
    while (*p++) len++;

    if (!left && width > len) {
        for (int i = 0; i < width - len; i++) snp_putc(o, ' ');
    }
    while (*s) snp_putc(o, *s++);
    if (left && width > len) {
        for (int i = 0; i < width - len; i++) snp_putc(o, ' ');
    }
}

static void snp_putnum(struct snp_out *o, unsigned long v, int base,
                       int upper, int width, int left, int zero)
{
    char tmp[32];
    int  n = 0;
    const char *digits_lo = "0123456789abcdef";
    const char *digits_up = "0123456789ABCDEF";
    const char *digits = upper ? digits_up : digits_lo;

    if (v == 0) {
        tmp[n++] = '0';
    } else {
        while (v) {
            tmp[n++] = digits[v % base];
            v /= base;
        }
    }
    // tmp сейчас в обратном порядке
    int len = n;
    char pad = zero && !left ? '0' : ' ';

    if (!left && width > len) {
        for (int i = 0; i < width - len; i++) snp_putc(o, pad);
    }
    for (int i = n - 1; i >= 0; i--) snp_putc(o, tmp[i]);
    if (left && width > len) {
        for (int i = 0; i < width - len; i++) snp_putc(o, ' ');
    }
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    struct snp_out o = { buf, size, 0 };

    va_list_t ap;
    va_start_t(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            snp_putc(&o, *p);
            continue;
        }
        p++;

        int left = 0, zero = 0;
        // Флаги
        while (*p == '-' || *p == '0') {
            if (*p == '-') left = 1;
            if (*p == '0') zero = 1;
            p++;
        }
        // Ширина
        int width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        switch (*p) {
            case 's': {
                const char *s = va_arg_t(ap, const char *);
                if (!s) s = "(null)";
                snp_puts(&o, s, width, left);
                break;
            }
            case 'c':
                snp_putc(&o, (char)va_arg_t(ap, int));
                break;
            case 'd':
            case 'i': {
                int v = va_arg_t(ap, int);
                unsigned long uv;
                int neg = 0;
                if (v < 0) { neg = 1; uv = (unsigned long)(-(long)v); }
                else       { uv = (unsigned long)v; }
                // Знак
                int saved_width = width;
                if (neg) {
                    snp_putc(&o, '-');
                    if (width > 0) width--;
                }
                snp_putnum(&o, uv, 10, 0, width, left, zero);
                (void)saved_width;
                break;
            }
            case 'u':
                snp_putnum(&o, va_arg_t(ap, unsigned int), 10, 0, width, left, zero);
                break;
            case 'x':
                snp_putnum(&o, va_arg_t(ap, unsigned int), 16, 0, width, left, zero);
                break;
            case 'X':
                snp_putnum(&o, va_arg_t(ap, unsigned int), 16, 1, width, left, zero);
                break;
            case 'p': {
                unsigned long v = (unsigned long)va_arg_t(ap, void *);
                snp_putc(&o, '0');
                snp_putc(&o, 'x');
                snp_putnum(&o, v, 16, 0, 8, 0, 1);
                break;
            }
            case '%':
                snp_putc(&o, '%');
                break;
            default:
                // Неизвестный спецификатор — выводим как есть
                if (*p) snp_putc(&o, *p);
                break;
        }
    }
    va_end_t(ap);

    // Завершаем '\0' в пределах буфера
    if (o.cap > 0) {
        size_t end = o.pos < o.cap - 1 ? o.pos : o.cap - 1;
        o.buf[end] = '\0';
    }
    return (int)o.pos;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list_t ap)
{
    // CherryUSB напрямую vsnprintf не зовёт, но на всякий случай.
    // Реализация — через временный буфер, обходя va_list нельзя,
    // поэтому обёртка бессмысленна. Убираем — если понадобится,
    // сделаем переработку snprintf через отдельный v-вариант.
    (void)buf; (void)size; (void)fmt; (void)ap;
    return 0;
}
