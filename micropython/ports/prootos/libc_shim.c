/* ports/prootos/libc_shim.c
 * string.h + assert + readline + re1.5 stubs для MicroPython.
 */

#include <stddef.h>
#include "py/mpconfig.h"
#include "py/misc.h"
#include "shared/readline/readline.h"

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

/* ─── fortify shims ─────────────────────────────── */

void* __memcpy_chk(void* dst, const void* src, size_t n, size_t dstlen) {
    (void)dstlen; return memcpy(dst, src, n);
}
void* __memset_chk(void* dst, int c, size_t n, size_t dstlen) {
    (void)dstlen; return memset(dst, c, n);
}
void* __memmove_chk(void* dst, const void* src, size_t n, size_t dstlen) {
    (void)dstlen; return memmove(dst, src, n);
}
char* __strcpy_chk(char* dst, const char* src, size_t dstlen) {
    (void)dstlen; return strcpy(dst, src);
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

                                      /* ─── re1.5 stubs ────────────────────────────────────
                                       * recursiveloop.c зовёт re1_5_fatal. В оригинале она живёт в dumpcode.c,
                                       * но мы его не подключаем (тянет printf). Заглушка — виснем,
                                       * потому что сюда попадаем только при внутренней ошибке regex. */
                                      void re1_5_fatal(const char *msg);
                                      void re1_5_fatal(const char *msg) {
                                          (void)msg;
                                          for (;;) { }
                                      }

                                      /* ─── readline с line-редактором + история ────────── */

                                      extern int sys_write(int fd, const void* buf, unsigned int len);
                                      extern int sys_read(int fd, void* buf, unsigned int len);

                                      #define KBD_KEY_UP     0x80
                                      #define KBD_KEY_DOWN   0x81
                                      #define KBD_KEY_LEFT   0x82
                                      #define KBD_KEY_RIGHT  0x83
                                      #define KBD_KEY_HOME   0x84
                                      #define KBD_KEY_END    0x85
                                      #define KBD_KEY_DEL    0x86

                                      #define HIST_SIZE      16
                                      #define HIST_LINE_MAX  128

                                      static char hist[HIST_SIZE][HIST_LINE_MAX];
                                      static int  hist_count = 0;

                                      static void hist_add(const char* s) {
                                          if (!s[0]) return;
                                          if (hist_count > 0) {
                                              int last = (hist_count - 1) % HIST_SIZE;
                                              if (strcmp(hist[last], s) == 0) return;
                                          }
                                          int idx = hist_count % HIST_SIZE;
                                          strcpy(hist[idx], s);
                                          hist_count++;
                                      }

                                      /* Индекс начала ТЕКУЩЕЙ строки — после последнего '\n'. */
                                      static unsigned int current_line_start(vstr_t *line) {
                                          for (unsigned int i = line->len; i > 0; i--) {
                                              if (line->buf[i - 1] == '\n') return i;
                                          }
                                          return 0;
                                      }

                                      static void redraw_line(const char *prompt, vstr_t *line, unsigned int pos) {
                                          unsigned int start   = current_line_start(line);
                                          unsigned int cur_len = line->len - start;
                                          unsigned int local   = (pos >= start) ? (pos - start) : 0;

                                          sys_write(1, "\r", 1);

                                          unsigned int plen = 0;
                                          while (prompt[plen]) plen++;
                                          sys_write(1, prompt, plen);

                                          if (cur_len > 0) sys_write(1, line->buf + start, cur_len);
                                          sys_write(1, " ", 1);

                                          unsigned int back = cur_len + 1 - local;
                                          for (unsigned int i = 0; i < back; i++) sys_write(1, "\b", 1);
                                      }

                                      static void replace_line(vstr_t *line, const char* s) {
                                          line->len = 0;
                                          for (unsigned int i = 0; s[i]; i++) vstr_add_char(line, s[i]);
                                      }

                                      int readline(vstr_t *line, const char *prompt) {
                                          unsigned int plen = 0;
                                          while (prompt[plen]) plen++;
                                          sys_write(1, prompt, plen);

                                          unsigned int pos = line->len;

                                          int is_ps1 = (line->len == 0);
                                          static char draft[HIST_LINE_MAX];
                                          int  draft_active = 0;
                                          int  hist_pos     = hist_count;

                                          for (;;) {
                                              char c;
                                              while (sys_read(0, &c, 1) <= 0) { }

                                              if (c == 0x03) { sys_write(1, "\r\n", 2); return CHAR_CTRL_C; }
                                              if (c == 0x04) { sys_write(1, "\r\n", 2); return CHAR_CTRL_D; }

                                              if (c == '\n') c = '\r';
                                              if (c == '\r') {
                                                  sys_write(1, "\r\n", 2);
                                                  if (is_ps1 && line->len > 0) {
                                                      char tmp[HIST_LINE_MAX];
                                                      unsigned int n = line->len;
                                                      if (n > HIST_LINE_MAX - 1) n = HIST_LINE_MAX - 1;
                                                      memcpy(tmp, line->buf, n);
                                                      tmp[n] = 0;
                                                      hist_add(tmp);
                                                  }
                                                  return 0;
                                              }

                                              unsigned char uc = (unsigned char) c;
                                              unsigned int  start = current_line_start(line);

                                              /* Backspace */
                                              if (uc == 0x08 || uc == 0x7F) {
                                                  if (pos <= start) continue;
                                                  for (unsigned int i = pos - 1; i + 1 < line->len; i++) {
                                                      line->buf[i] = line->buf[i + 1];
                                                  }
                                                  line->len--;
                                                  pos--;
                                                  redraw_line(prompt, line, pos);
                                                  continue;
                                              }

                                              /* Delete */
                                              if (uc == KBD_KEY_DEL) {
                                                  if (pos >= line->len) continue;
                                                  if (line->buf[pos] == '\n') continue;
                                                  for (unsigned int i = pos; i + 1 < line->len; i++) {
                                                      line->buf[i] = line->buf[i + 1];
                                                  }
                                                  line->len--;
                                                  redraw_line(prompt, line, pos);
                                                  continue;
                                              }

                                              /* ← */
                                              if (uc == KBD_KEY_LEFT) {
                                                  if (pos > start) { pos--; sys_write(1, "\b", 1); }
                                                  continue;
                                              }

                                              /* → */
                                              if (uc == KBD_KEY_RIGHT) {
                                                  if (pos < line->len) {
                                                      sys_write(1, &line->buf[pos], 1);
                                                      pos++;
                                                  }
                                                  continue;
                                              }

                                              /* Home */
                                              if (uc == KBD_KEY_HOME) {
                                                  while (pos > start) { sys_write(1, "\b", 1); pos--; }
                                                  continue;
                                              }

                                              /* End */
                                              if (uc == KBD_KEY_END) {
                                                  while (pos < line->len) {
                                                      sys_write(1, &line->buf[pos], 1);
                                                      pos++;
                                                  }
                                                  continue;
                                              }

                                              /* ↑ — история */
                                              if (uc == KBD_KEY_UP) {
                                                  if (!is_ps1) continue;
                                                  if (!draft_active) {
                                                      unsigned int n = line->len;
                                                      if (n > HIST_LINE_MAX - 1) n = HIST_LINE_MAX - 1;
                                                      memcpy(draft, line->buf, n);
                                                      draft[n] = 0;
                                                      draft_active = 1;
                                                  }
                                                  if (hist_pos > 0) {
                                                      hist_pos--;
                                                      replace_line(line, hist[hist_pos % HIST_SIZE]);
                                                      pos = line->len;
                                                      redraw_line(prompt, line, pos);
                                                  }
                                                  continue;
                                              }

                                              /* ↓ — история */
                                              if (uc == KBD_KEY_DOWN) {
                                                  if (!is_ps1 || !draft_active) continue;
                                                  if (hist_pos < hist_count - 1) {
                                                      hist_pos++;
                                                      replace_line(line, hist[hist_pos % HIST_SIZE]);
                                                  } else {
                                                      hist_pos = hist_count;
                                                      replace_line(line, draft);
                                                      draft_active = 0;
                                                  }
                                                  pos = line->len;
                                                  redraw_line(prompt, line, pos);
                                                  continue;
                                              }

                                              /* Управляющие — игнор */
                                              if ((uc < 0x20) && (uc != '\t')) continue;

                                              /* Вставка символа */
                                              vstr_add_char(line, c);
                                              for (unsigned int i = line->len - 1; i > pos; i--) {
                                                  line->buf[i] = line->buf[i - 1];
                                              }
                                              line->buf[pos] = c;
                                              pos++;
                                              redraw_line(prompt, line, pos);
                                          }
                                      }
