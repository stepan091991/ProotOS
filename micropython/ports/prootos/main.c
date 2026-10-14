#include <stdint.h>
#include <string.h>

#include "py/builtin.h"
#include "py/compile.h"
#include "py/lexer.h"
#include "py/reader.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/mpprint.h"
#include "py/misc.h"
#include "shared/runtime/pyexec.h"

static char *stack_top;

#if MICROPY_ENABLE_GC
static char heap[MICROPY_HEAP_SIZE];
#endif

extern void sys_exit(int code);
extern int  sys_open(const char* path, int flags);
extern int  sys_read(int fd, void* buf, unsigned int len);
extern int  sys_close(int fd);

int main(int argc, char **argv);

/* _start — точка входа ELF. ABI: [esp]=argc, [esp+4]=argv[]. */
__asm__(
    ".text\n"
    ".globl _start\n"
    ".type _start, @function\n"
    "_start:\n"
    "    movl  (%esp), %eax\n"
    "    leal  4(%esp), %edx\n"
    "    pushl %edx\n"
    "    pushl %eax\n"
    "    call  main\n"
    "    pushl %eax\n"
    "    call  sys_exit\n"
    "_start_hang:\n"
    "    hlt\n"
    "    jmp   _start_hang\n"
    ".size _start, . - _start\n"
);

int main(int argc, char **argv) {
    int stack_dummy;
    stack_top = (char *)&stack_dummy;

    #if MICROPY_ENABLE_GC
    gc_init(heap, heap + sizeof(heap));
    #endif

    mp_init();

    if (argc >= 2 && argv[1][0] != '-') {
        /* exec micropy.elf /path/to/script.py */
        mp_printf(&mp_plat_print, "[micropy] running %s\r\n", argv[1]);
        int r = pyexec_file(argv[1]);
        mp_printf(&mp_plat_print, "[micropy] exit code %d\r\n", r);
    } else if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        vstr_t vstr;
        vstr_init(&vstr, 32);
        vstr_add_strn(&vstr, argv[2], strlen(argv[2]));
        pyexec_vstr(&vstr, true);
        vstr_clear(&vstr);
    } else {
        pyexec_friendly_repl();
    }

    mp_deinit();
    return 0;
}

#if MICROPY_ENABLE_GC
void gc_collect(void) {
    void *dummy;
    gc_collect_start();
    gc_collect_root(&dummy,
                    ((mp_uint_t)stack_top - (mp_uint_t)&dummy) / sizeof(mp_uint_t));
    gc_collect_end();
}
#endif

/* ─── Файловый ввод для lexer ─────────────────── */

typedef struct _mp_reader_prootos_t {
    int  fd;
    int  pos;
    int  len;
    char buf[256];
} mp_reader_prootos_t;

static mp_uint_t mp_reader_prootos_readbyte(void *data) {
    mp_reader_prootos_t *r = (mp_reader_prootos_t *)data;
    if (r->pos >= r->len) {
        int n = sys_read(r->fd, r->buf, sizeof(r->buf));
        if (n <= 0) return MP_READER_EOF;
        r->pos = 0;
        r->len = n;
    }
    return (mp_uint_t)(unsigned char) r->buf[r->pos++];
}

static void mp_reader_prootos_close(void *data) {
    mp_reader_prootos_t *r = (mp_reader_prootos_t *)data;
    sys_close(r->fd);
    m_del_obj(mp_reader_prootos_t, r);
}

mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    const char *path = qstr_str(filename);
    int fd = sys_open(path, 0);
    if (fd < 0) {
        mp_raise_OSError(MP_ENOENT);
        return NULL;
    }

    mp_reader_prootos_t *r = m_new_obj(mp_reader_prootos_t);
    r->fd  = fd;
    r->pos = 0;
    r->len = 0;

    mp_reader_t reader;
    reader.data     = r;
    reader.readbyte = mp_reader_prootos_readbyte;
    reader.close    = mp_reader_prootos_close;

    return mp_lexer_new(filename, reader);
}

mp_import_stat_t mp_import_stat(const char *path) {
    int fd = sys_open(path, 0);
    if (fd < 0) return MP_IMPORT_STAT_NO_EXIST;
    sys_close(fd);
    return MP_IMPORT_STAT_FILE;
}
