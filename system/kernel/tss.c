#include "tss.h"
#include "gdt.h"
#include "printf.h"
#include "string.h"

struct tss_entry {
    u32 prev_tss;
    u32 esp0;         /* стек ядра (ring 0) */
    u32 ss0;          /* сегмент стека ядра */
    u32 esp1, ss1;
    u32 esp2, ss2;
    u32 cr3;
    u32 eip, eflags;
    u32 eax, ecx, edx, ebx;
    u32 esp, ebp, esi, edi;
    u32 es, cs, ss, ds, fs, gs;
    u32 ldt;
    u16 trap;
    u16 iomap_base;
} __attribute__((packed));

static struct tss_entry tss;

/* Kernel stack — 16 КБ в .bss. */
static u8 kernel_stack[16384] __attribute__((aligned(16)));

void tss_init(void) {
    memset(&tss, 0, sizeof(tss));

    tss.ss0        = GDT_KERNEL_DATA;
    tss.esp0       = (u32)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(tss);

    /* Добавляем дескриптор TSS в GDT (индекс 5).
     * Access 0x89 = present, ring 0, тип 32-bit TSS available.
     * Granularity 0x00 = байты, 16-битный размер — для TSS это ок. */
    gdt_set_entry(5, (u32) &tss, sizeof(tss) - 1, 0x89, 0, 0);

    /* Загружаем селектор в TR. */
    __asm__ volatile ("ltr %0" : : "r"((u16) GDT_TSS_SELECTOR));

    printf("[tss] loaded, esp0=%p\n", (void*) tss.esp0);
}

void tss_set_kernel_stack(u32 esp0) {
    tss.esp0 = esp0;
}
