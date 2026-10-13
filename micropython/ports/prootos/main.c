/* ports/prootos/main.c */

#include <stdint.h>
#include <string.h>

#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "shared/runtime/pyexec.h"

static char *stack_top;

#if MICROPY_ENABLE_GC
static char heap[MICROPY_HEAP_SIZE];
#endif

extern void sys_exit(int code);

int main(int argc, char **argv);

void _start(void) {
    main(0, NULL);
    sys_exit(0);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int stack_dummy;
    stack_top = (char *)&stack_dummy;

    #if MICROPY_ENABLE_GC
    gc_init(heap, heap + sizeof(heap));
    #endif

    mp_init();
    pyexec_friendly_repl();
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

mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    (void)filename;
    mp_raise_OSError(MP_ENOENT);
    return NULL;
}

mp_import_stat_t mp_import_stat(const char *path) {
    (void)path;
    return MP_IMPORT_STAT_NO_EXIST;
}