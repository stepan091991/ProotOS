/* ports/prootos/libc_shim.c
 * string.h + assert + readline для MicroPython.
 */

#include <stddef.h>
#include "py/mpconfig.h"
#include "py/misc.h"      /* vstr_t, vstr_add_char */
#include "shared/readline/readline.h"   /* CHAR_CTRL_C / CHAR_CTRL_D */

/* ─── string.h ────────────────────────────────────── */

void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*) dst;
    const unsigned char* s = (const unsigned char*) src;
    while (n--) *d++ = *s++;
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*) dst;
    const unsigned char* s = (const unsigned char*) src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*) s;
    while (n--) *p++ = (unsigned char) c;
    return s;
}

int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char* pa = (const unsigned char*) a;
    const unsigned char* pb = (const unsigned char*) b;
    while (n--) {
        if (*pa != *pb) return (int)(*pa) - (int)(*pb);
        pa++; pb++;
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char) c) return (char*) s;
        s++;
    }
    return (c == 0) ? (char*) s : NULL;
}

char* strcpy(char* dst, const char* src) {
    char* ret = dst;
    while ((*dst++ = *src++) != '\0') { }
    return ret;
}

/* ─── fortify shims для -O2 на i386 ─────────────── */

void* __memcpy_chk(void* dst, const void* src, size_t n, size_t dstlen) {
    (void)dstlen;
    return memcpy(dst, src, n);
}
void* __memset_chk(void* dst, int c, size_t n, size_t dstlen) {
    (void)dstlen;
    return memset(dst, c, n);
}
void* __memmove_chk(void* dst, const void* src, size_t n, size_t dstlen) {
    (void)dstlen;
    return memmove(dst, src, n);
}
char* __strcpy_chk(char* dst, const char* src, size_t dstlen) {
    (void)dstlen;
    return strcpy(dst, src);
}

/* ─── assert / fatal ─────────────────────────────── */

void __assert_fail(const char* assertion, const char* file,
                   unsigned int line, const char* function) {
    (void)assertion; (void)file; (void)line; (void)function;
    for (;;) { }
                   }

                   void __assert_func(const char* file, int line, const char* func,
                                      const char* expr) {
                       (void)file; (void)line; (void)func; (void)expr;
                       for (;;) { }
                                      }

                                      void nlr_jump_fail(void *val) {
                                          (void)val;
                                          for (;;) { }
                                      }

                                      /* ─── readline для MicroPython REPL ─────────────── */

                                      extern int sys_write(int fd, const void* buf, unsigned int len);
                                      extern int sys_read(int fd, void* buf, unsigned int len);

                                      int readline(vstr_t *line, const char *prompt) {
                                          unsigned int plen = 0;
                                          while (prompt[plen]) plen++;
                                          sys_write(1, prompt, plen);

                                          for (;;) {
                                              char c;
                                              while (sys_read(0, &c, 1) <= 0) { }

                                              if (c == 0x03) { sys_write(1, "\r\n", 2); return CHAR_CTRL_C; }
                                              if (c == 0x04) { sys_write(1, "\r\n", 2); return CHAR_CTRL_D; }

                                              if (c == '\n') c = '\r';

                                              if (c == '\r') {
                                                  sys_write(1, "\r\n", 2);   /* <── завершаем эхо-строку */
                                                  break;
                                              }

                                              if (c == '\b' || c == 0x7F) {
                                                  if (line->len > 0) {
                                                      line->len--;
                                                      sys_write(1, "\b \b", 3);
                                                  }
                                                  continue;
                                              }

                                              vstr_add_char(line, c);
                                              sys_write(1, &c, 1);
                                          }

                                          return 0;
                                      }
