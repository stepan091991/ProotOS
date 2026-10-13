/* ports/prootos/mpconfigport.h */

#include <stdint.h>

#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_MINIMUM)

#define MICROPY_ENABLE_COMPILER     (1)
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_HELPER_REPL         (1)
#define MICROPY_PY_USE_READLINE     (1)   /* встроенная readline */
#define MICROPY_MODULE_FROZEN_MPY   (0)
#define MICROPY_ENABLE_EXTERNAL_IMPORT    (0)

#define MICROPY_ALLOC_PATH_MAX            (256)
#define MICROPY_ALLOC_PARSE_CHUNK_INIT    (16)

#define MICROPY_PY_SYS_MODULES            (0)
#define MICROPY_PY_SYS_EXIT               (0)
#define MICROPY_PY_SYS_PATH               (0)
#define MICROPY_PY_SYS_ARGV               (0)

typedef long mp_off_t;

#define MICROPY_HEAP_SIZE      (65536)

#define MICROPY_HW_BOARD_NAME "prootos"
#define MICROPY_HW_MCU_NAME   "i386"

#define MP_STATE_PORT MP_STATE_VM

#include <alloca.h>