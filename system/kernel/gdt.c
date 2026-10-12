#include "gdt.h"
#include "string.h"
#include "printf.h"

/* ─────────────────────────────────────────────────────────
 *  Описание одной записи GDT (8 байт).
 *  Структура упакована — компилятор не должен вставлять padding.
 * ───────────────────────────────────────────────────────── */
struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  granularity;   /* старшие 4 бита — flags, младшие 4 — limit */
    u8  base_high;
} __attribute__((packed));

/* То, что загружается в GDTR */
struct gdt_ptr {
    u16 limit;         /* размер GDT минус 1 */
    u32 base;          /* линейный адрес GDT */
} __attribute__((packed));

/* Наша таблица. 5 записей: null + kernel code + kernel data + user code + user data */
static struct gdt_entry gdt_entries[6];
static struct gdt_ptr   gdt_ptr;

/* Внешняя функция на ассемблере: грузит GDTR и перезагружает сегменты */
extern void gdt_flush(u32 gdt_ptr_addr);

/* ─────────────────────────────────────────────────────────
 *  Заполнение одной записи GDT.
 *
 *  base        — линейный адрес начала сегмента
 *  limit       — размер сегмента (в байтах, но см. флаг gran_4k)
 *  access      — байт прав доступа
 *  gran_4k     — 1 = limit измеряется в страницах 4 КБ
 *  is_32bit    — 1 = 32-битный сегмент (для кода/данных — всегда 1)
 * ───────────────────────────────────────────────────────── */
void gdt_set_entry(int index, u32 base, u32 limit,
                          u8 access, int gran_4k, int is_32bit)
{
    gdt_entries[index].base_low    = (u16)(base & 0xFFFF);
    gdt_entries[index].base_mid    = (u8)((base >> 16) & 0xFF);
    gdt_entries[index].base_high   = (u8)((base >> 24) & 0xFF);

    gdt_entries[index].limit_low   = (u16)(limit & 0xFFFF);
    gdt_entries[index].granularity = (u8)((limit >> 16) & 0x0F);

    /* Старшие 4 бита granularity — это flags:
     *   бит 7: Granularity (0 = байты, 1 = страницы 4K)
     *   бит 6: D/B (0 = 16-бит, 1 = 32-бит)
     *   бит 5: Long mode (для x86_64, у нас 0)
     *   бит 4: AVL (свободно для ОС, у нас 0)
     */
    u8 flags = 0;
    if (gran_4k)  flags |= 0x80;
    if (is_32bit) flags |= 0x40;
    gdt_entries[index].granularity |= flags;

    gdt_entries[index].access = access;
}

/* Байты доступа для наших сегментов.
 * Формат access byte:
 *   бит 7: Present (1 = сегмент есть в памяти)
 *   биты 6..5: DPL (Ring 0..3)
 *   бит 4: S (1 = код/данные, 0 = системный, например TSS)
 *   бит 3: E (0 = данные, 1 = код)
 *   бит 2: DC (для кода: conforming; для данных: expand-down) — обычно 0
 *   бит 1: RW (для кода: readable; для данных: writable)
 *   бит 0: A (Accessed, ставится процессором) — 0
 */

/* kernel code: present, ring 0, code, readable   = 1001 1010 = 0x9A */
#define GDT_KERNEL_CODE_ACCESS 0x9A
/* kernel data: present, ring 0, data, writable   = 1001 0010 = 0x92 */
#define GDT_KERNEL_DATA_ACCESS 0x92
/* user   code: present, ring 3, code, readable   = 1111 1010 = 0xFA */
#define GDT_USER_CODE_ACCESS   0xFA
/* user   data: present, ring 3, data, writable   = 1111 0010 = 0xF2 */
#define GDT_USER_DATA_ACCESS   0xF2

void gdt_init(void) {
    gdt_ptr.limit = sizeof(gdt_entries) - 1;
    gdt_ptr.base  = (u32) &gdt_entries;

    /* Индекс 0: Null descriptor — обязателен по спецификации. */
    gdt_set_entry(0, 0, 0, 0, 0, 0);

    /* Индекс 1: Kernel code, base=0, limit=4G (в страницах 4K это 0xFFFFF). */
    gdt_set_entry(1, 0, 0xFFFFF, GDT_KERNEL_CODE_ACCESS, 1, 1);

    /* Индекс 2: Kernel data — то же самое, но access = data. */
    gdt_set_entry(2, 0, 0xFFFFF, GDT_KERNEL_DATA_ACCESS, 1, 1);

    /* Индекс 3: User code. */
    gdt_set_entry(3, 0, 0xFFFFF, GDT_USER_CODE_ACCESS, 1, 1);

    /* Индекс 4: User data. */
    gdt_set_entry(4, 0, 0xFFFFF, GDT_USER_DATA_ACCESS, 1, 1);

    /* Загружаем GDT в процессор. */
    gdt_flush((u32) &gdt_ptr);

    printf("[gdt] loaded %u entries, limit=%u, base=%p\n",
           5u, (u32) gdt_ptr.limit, (void*) gdt_ptr.base);
}
