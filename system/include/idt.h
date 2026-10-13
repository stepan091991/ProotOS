#ifndef PROOTOS_IDT_H
#define PROOTOS_IDT_H

#include "types.h"

/* Флаги для IDT-записи */
#define IDT_FLAG_PRESENT    0x80
#define IDT_FLAG_RING0      0x00
#define IDT_FLAG_RING3      0x60
#define IDT_FLAG_INT_GATE   0x0E   /* 32-bit interrupt gate */
#define IDT_FLAG_TRAP_GATE  0x0F   /* 32-bit trap gate */

/* Стандартные комбинации */
#define IDT_INT_GATE_RING0  (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INT_GATE)   /* 0x8E */
#define IDT_INT_GATE_RING3  (IDT_FLAG_PRESENT | IDT_FLAG_RING3 | IDT_FLAG_INT_GATE)   /* 0xEE */
#define IDT_TRAP_GATE_RING0 (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_TRAP_GATE)  /* 0x8F */

void idt_init(void);
void idt_set_gate(u8 num, u32 handler, u16 selector, u8 flags);

#endif
