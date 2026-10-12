#include "pit.h"
#include "printf.h"
#include "isr.h"
#include "pic.h"
#include "process.h"

#define PIT_CHANNEL0   0x40
#define PIT_COMMAND    0x43
#define PIT_BASE_FREQ  1193182

static volatile u64 ticks = 0;

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void pit_handler(registers_t* regs) {
    (void) regs;
    ticks++;
    scheduler_tick();       /* ставит need_resched */
    pic_send_eoi(0);
}

void pit_init(u32 frequency) {
    if (frequency == 0) frequency = 100;
    u32 divisor = PIT_BASE_FREQ / frequency;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (u8)((divisor >> 8) & 0xFF));

    isr_register_handler(32, pit_handler);
    pic_clear_mask(0);

    printf("[pit] initialized at %u Hz (divisor=%u)\n",
           frequency, divisor);
}

u64 pit_get_ticks(void) {
    return ticks;
}