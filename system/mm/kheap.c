#include "kheap.h"
#include "pmm.h"
#include "printf.h"
#include "string.h"

/* ─── Настройки ───────────────────────────────────────── */

#define HEAP_INITIAL_PAGES  16      /* первый блок при init = 64 КБ */
#define HEAP_MAX_PAGES      256     /* предел роста = 1 МБ */
#define ALIGN8(x)           (((x) + 7) & ~7u)

/* Магическое число для проверки целостности заголовка.
 * "KHEP" в hex — легко заметить в дампе памяти. */
#define KHEAP_MAGIC         0x4B484550

/* ─── Структура заголовка блока ──────────────────────── */

typedef struct block_header {
    u32 magic;              /* KHEAP_MAGIC — проверка целостности */
    u32 size;               /* размер данных (без заголовка) */
    u32 is_free;            /* 1 = свободен, 0 = занят */
    u32 reserved;           /* выравнивание до 16 байт */
} __attribute__((packed)) block_header_t;

#define HEADER_SIZE  (sizeof(block_header_t))

/* ─── Глобальное состояние heap ──────────────────────── */

static u8*  heap_start  = NULL;
static u8*  heap_end    = NULL;    /* адрес ПЕРВОГО байта после heap */
static u32  heap_pages  = 0;       /* сколько страниц выдано PMM */

/* ─── Низкоуровневые операции над блоками ────────────── */

static block_header_t* first_block(void) {
    return (block_header_t*) heap_start;
}

static block_header_t* next_block(block_header_t* b) {
    return (block_header_t*)((u8*) b + HEADER_SIZE + b->size);
}

static void write_header(block_header_t* b, u32 size, u32 is_free) {
    b->magic    = KHEAP_MAGIC;
    b->size     = size;
    b->is_free  = is_free;
    b->reserved = 0;
}

/* ─── Инициализация ──────────────────────────────────── */

void kheap_init(void) {
    heap_start = NULL;
    heap_end   = NULL;
    heap_pages = 0;

    /* Забираем у PMM несколько страниц подряд.
     * Так как PMM выдаёт страницы в возрастающем порядке, они
     * будут непрерывны, если только кто-то до нас не взял
     * страницу в этом диапазоне. */
    void* first = NULL;
    void* prev  = NULL;
    u32   got   = 0;

    for (u32 i = 0; i < HEAP_INITIAL_PAGES; i++) {
        void* p = pmm_alloc_page();
        if (!p) break;

        if (i == 0) {
            first = p;
        } else {
            /* Проверяем непрерывность */
            if ((u8*) p != (u8*) prev + PMM_PAGE_SIZE) {
                pmm_free_page(p);
                break;
            }
        }
        prev = p;
        got++;
    }

    if (got == 0) {
        printf("[kheap] ERROR: could not allocate any pages!\n");
        return;
    }

    heap_start = (u8*) first;
    heap_end   = (u8*) first + got * PMM_PAGE_SIZE;
    heap_pages = got;

    /* Инициализируем единственный свободный блок на всю кучу */
    block_header_t* b = first_block();
    write_header(b, (u32)(heap_end - heap_start) - HEADER_SIZE, 1);

    printf("[kheap] initialized: 0x%p - 0x%p (%u pages, %u KB)\n",
           heap_start, heap_end, heap_pages, heap_pages * 4);
}

/* ─── Расширение heap ────────────────────────────────── */

static int heap_extend(void) {
    if (heap_pages >= HEAP_MAX_PAGES) {
        return 0;   /* достигли лимита */
    }

    void* p = pmm_alloc_page();
    if (!p) return 0;

    /* Требуем непрерывности с текущим heap */
    if ((u8*) p != heap_end) {
        pmm_free_page(p);
        return 0;
    }

    /* Превращаем предыдущий последний блок (если он свободен)
     * в более крупный, добавив новую страницу. */
    u8* new_end = heap_end + PMM_PAGE_SIZE;

    /* Обходим до последнего блока */
    block_header_t* b = first_block();
    for (;;) {
        block_header_t* n = next_block(b);
        if ((u8*) n >= heap_end) break;
        b = n;
    }

    /* b — последний блок. Если свободен — увеличиваем его size. */
    if (b->is_free) {
        b->size += PMM_PAGE_SIZE;
    } else {
        /* Иначе создаём новый блок сразу после него */
        block_header_t* nb = (block_header_t*) heap_end;
        write_header(nb, PMM_PAGE_SIZE - HEADER_SIZE, 1);
    }

    heap_end    = new_end;
    heap_pages += 1;
    return 1;
}

/* ─── Поиск свободного блока ─────────────────────────── */

static block_header_t* find_free_block(u32 size) {
    if (!heap_start) return NULL;

    u8* p = heap_start;
    while (p < heap_end) {
        block_header_t* b = (block_header_t*) p;
        if (b->magic != KHEAP_MAGIC) {
            printf("[kheap] PANIC: bad magic at %p (got 0x%x)\n",
                   (void*) b, b->magic);
            return NULL;
        }
        if (b->is_free && b->size >= size) {
            return b;
        }
        p = (u8*) next_block(b);
    }
    return NULL;
}

/* ─── Разделение блока ───────────────────────────────── */

static void split_block(block_header_t* b, u32 size) {
    /* Отрезаем хвост и делаем его свободным блоком.
     * Разделяем, только если хвоста хватает на заголовок + 8 байт. */
    if (b->size < size + HEADER_SIZE + 8) {
        return;
    }

    u32 old_size   = b->size;
    u32 new_size   = size;
    u32 tail_size  = old_size - new_size - HEADER_SIZE;

    b->size = new_size;

    block_header_t* tail = (block_header_t*)((u8*) b + HEADER_SIZE + new_size);
    write_header(tail, tail_size, 1);
}

/* ─── Публичный API ──────────────────────────────────── */

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    if (!heap_start) {
        printf("[kheap] WARN: kmalloc before kheap_init\n");
        return NULL;
    }

    u32 want = ALIGN8((u32) size);

    block_header_t* b = find_free_block(want);
    if (!b) {
        /* Попробуем расширить heap и поискать ещё раз */
        if (!heap_extend()) {
            printf("[kheap] OOM: no memory for %u bytes\n", (u32) size);
            return NULL;
        }
        b = find_free_block(want);
        if (!b) {
            printf("[kheap] OOM after extend: no block for %u bytes\n", (u32) size);
            return NULL;
        }
    }

    split_block(b, want);
    b->is_free = 0;

    return (u8*) b + HEADER_SIZE;
}

void* kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void*  p     = kmalloc(total);
    if (p) {
        memset(p, 0, (u32) total);
    }
    return p;
}

void kfree(void* ptr) {
    if (!ptr) return;

    block_header_t* b = (block_header_t*)((u8*) ptr - HEADER_SIZE);

    if (b->magic != KHEAP_MAGIC) {
        printf("[kheap] WARN: kfree on bad pointer %p (magic=0x%x)\n",
               ptr, b->magic);
        return;
    }

    if (b->is_free) {
        printf("[kheap] WARN: double free at %p\n", ptr);
        return;
    }

    b->is_free = 1;

    /* Coalescing: пробегаемся от начала heap и сливаем все
     * соседние свободные блоки. Это O(n), но зато просто. */
    u8* p = heap_start;
    while (p < heap_end) {
        block_header_t* cur = (block_header_t*) p;
        block_header_t* nxt = next_block(cur);

        if ((u8*) nxt >= heap_end) break;

        if (cur->is_free && nxt->is_free) {
            cur->size += HEADER_SIZE + nxt->size;
            /* Не двигаем p — возможно, следующий тоже свободен. */
            continue;
        }
        p = (u8*) nxt;
    }
}

/* ─── Статистика ─────────────────────────────────────── */

void kheap_dump(void) {
    if (!heap_start) {
        printf("[kheap] not initialized\n");
        return;
    }

    printf("[kheap] region:   0x%p - 0x%p (%u KB, %u pages)\n",
           heap_start, heap_end, heap_pages * 4, heap_pages);

    u32 free_bytes  = 0;
    u32 used_bytes  = 0;
    u32 free_blocks = 0;
    u32 used_blocks = 0;
    u32 total_blocks = 0;

    u8* p = heap_start;
    while (p < heap_end) {
        block_header_t* b = (block_header_t*) p;
        total_blocks++;
        if (b->is_free) {
            free_bytes += b->size;
            free_blocks++;
        } else {
            used_bytes += b->size;
            used_blocks++;
        }
        p = (u8*) next_block(b);
    }

    printf("[kheap] blocks:   %u total (%u used, %u free)\n",
           total_blocks, used_blocks, free_blocks);
    printf("[kheap] bytes:    %u used, %u free, %u total\n",
           used_bytes, free_bytes, used_bytes + free_bytes);
}
