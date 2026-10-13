#include "pic.h"
#include "printf.h"

/* Порты 8259A */
#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

/* Команды инициализации */
#define ICW1_INIT  0x10
#define ICW1_ICW4  0x01
#define ICW4_8086  0x01

/* EOI */
#define PIC_EOI    0x20

/* ─── Низкоуровневый доступ к портам ─────────────────── */

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* Небольшая задержка — старые PIC'и требуют паузу между командами.
 * Пишем в неиспользуемый порт 0x80 — классический трюк. */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* ─── Инициализация ──────────────────────────────────── */

void pic_init(void) {
    /* Начинаем инициализацию обоих PIC'ов */
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2 — базовые векторы */
    outb(PIC1_DATA, PIC_MASTER_VECTOR);  /* IRQ0..7  → 32..39 */
    io_wait();
    outb(PIC2_DATA, PIC_SLAVE_VECTOR);   /* IRQ8..15 → 40..47 */
    io_wait();

    /* ICW3 — топология */
    outb(PIC1_DATA, 0x04);  /* slave подключён к IRQ2 master'а */
    io_wait();
    outb(PIC2_DATA, 0x02);  /* slave — это второй каскад */
    io_wait();

    /* ICW4 — режим 8086 */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Пока маскируем ВСЁ — мы размаскируем только то,
     * что нам нужно (таймер и клавиатуру), уже после этого. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    printf("[pic] remapped: master -> 32..39, slave -> 40..47\n");
}

void pic_send_eoi(u8 irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

/* ─── Маски ──────────────────────────────────────────── */

static u8 pic_master_mask = 0xFF;
static u8 pic_slave_mask  = 0xFF;

void pic_set_mask(u8 irq) {
    if (irq < 8) {
        pic_master_mask |=  (1 << irq);
        outb(PIC1_DATA, pic_master_mask);
    } else {
        pic_slave_mask  |=  (1 << (irq - 8));
        outb(PIC2_DATA, pic_slave_mask);
    }
}

void pic_clear_mask(u8 irq) {
    if (irq < 8) {
        pic_master_mask &= ~(1 << irq);
        outb(PIC1_DATA, pic_master_mask);
    } else {
        pic_slave_mask  &= ~(1 << (irq - 8));
        outb(PIC2_DATA, pic_slave_mask);
    }
}

void pic_mask_all(void) {
    pic_master_mask = 0xFF;
    pic_slave_mask  = 0xFF;
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
