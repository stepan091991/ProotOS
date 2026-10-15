#include <stdint.h>

#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_MINIMUM)

#define MICROPY_PERSISTENT_CODE_SAVE (0)
#define MICROPY_PERSISTENT_CODE_LOAD (0)

#define MICROPY_ENABLE_COMPILER     (1)
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_HELPER_REPL         (1)
#define MICROPY_MODULE_FROZEN_MPY   (0)
#define MICROPY_ENABLE_EXTERNAL_IMPORT    (1)

#define MICROPY_ALLOC_PATH_MAX            (256)
#define MICROPY_ALLOC_PARSE_CHUNK_INIT    (16)

#define MICROPY_PY_SYS_MODULES            (0)
#define MICROPY_PY_SYS_EXIT               (0)
#define MICROPY_PY_SYS_ARGV               (1)
#define MICROPY_PY_SYS_PATH               (1)

#define MICROPY_PY_IO                     (1)
#define MICROPY_PY_BUILTINS_BYTEARRAY     (1)
#define MICROPY_PY_BUILTINS_MEMORYVIEW    (1)

#define MICROPY_PY_BINASCII               (1)
#define MICROPY_PY_HEAPQ                  (1)
#define MICROPY_PY_JSON                   (1)
#define MICROPY_PY_UCTYPES                (1)
#define MICROPY_PY_FRAMEBUF               (1)
#define MICROPY_PY_PLATFORM               (1)
#define MICROPY_PY_RANDOM                 (1)
#define MICROPY_PY_RANDOM_EXTRA_FUNCS     (1)

/* ─── hashlib: только SHA256 ─── */
#define MICROPY_PY_HASHLIB                (1)
#define MICROPY_PY_HASHLIB_MD5            (0)
#define MICROPY_PY_HASHLIB_SHA1           (0)
#define MICROPY_PY_HASHLIB_SHA256         (1)

/* ─── re ─── */
#define MICROPY_PY_RE                     (1)

/* ─── deflate ─── */
#define MICROPY_PY_DEFLATE                (1)
#define MICROPY_PY_DEFLATE_COMPRESS       (0)

/* ─── time ─── */
#define MICROPY_PY_TIME                   (1)
#define MICROPY_PY_TIME_MS                (1)
#define MICROPY_PY_TIME_UPTIME_MS         (1)
#define MICROPY_PY_TIME_TICKS             (1)

#define MICROPY_PY_MARSHAL                (0)

typedef long mp_off_t;

#define MICROPY_HEAP_SIZE      (131072)

#define MICROPY_HW_BOARD_NAME "prootos"
#define MICROPY_HW_MCU_NAME   "i386"

#define MP_STATE_PORT MP_STATE_VM

#include <alloca.h>
