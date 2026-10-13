#include "idt.h"
#include "gdt.h"
#include "string.h"
#include "printf.h"

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8  zero;
    u8  flags;
    u16 offset_high;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

static struct idt_entry idt_entries[256];
static struct idt_ptr   idt_ptr_;

extern void idt_flush(u32 idt_ptr_addr);

/* Таблицы адресов стабов — определены в isr_stubs.S */
extern u32 isr_stub_table[];
extern u32 irq_stub_table[];

void idt_set_gate(u8 num, u32 handler, u16 selector, u8 flags) {
    idt_entries[num].offset_low  = (u16)(handler & 0xFFFF);
    idt_entries[num].offset_high = (u16)((handler >> 16) & 0xFFFF);
    idt_entries[num].selector    = selector;
    idt_entries[num].zero        = 0;
    idt_entries[num].flags       = flags;
}

void idt_init(void) {
    idt_ptr_.limit = sizeof(idt_entries) - 1;
    idt_ptr_.base  = (u32) &idt_entries;

    /* Обнуляем всю таблицу */
    memset(idt_entries, 0, sizeof(idt_entries));

    /* Исключения 0..31 */
    for (u8 i = 0; i < 32; i++) {
        idt_set_gate(i, isr_stub_table[i], GDT_KERNEL_CODE, IDT_INT_GATE_RING0);
    }

    /* IRQ 0..15 → векторы 32..47 (после ремапа PIC) */
    for (u8 i = 0; i < 16; i++) {
        idt_set_gate(32 + i, irq_stub_table[i], GDT_KERNEL_CODE, IDT_INT_GATE_RING0);
    }

    /* Векторы 48..255 остаются неинициализированными (not present) */

    idt_flush((u32) &idt_ptr_);

    printf("[idt] loaded 256 entries, %u gates installed, limit=%u\n",
           48u, (u32) idt_ptr_.limit);
}
