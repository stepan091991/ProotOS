#include "serial.h"

/* Порты COM1 */
#define PORT_DATA       (SERIAL_COM1 + 0)
#define PORT_IER        (SERIAL_COM1 + 1)
#define PORT_FIFO       (SERIAL_COM1 + 2)
#define PORT_LCR        (SERIAL_COM1 + 3)
#define PORT_MCR        (SERIAL_COM1 + 4)
#define PORT_LSR        (SERIAL_COM1 + 5)

static int serial_ready = 0;

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void) {
    /* Отключаем прерывания от serial — работаем в режиме polling. */
    outb(PORT_IER, 0x00);

    /* Скорость: сначала выставляем DLAB=1 (бит 7 в LCR),
     * чтобы получить доступ к регистрам делителя. */
    outb(PORT_LCR, 0x80);

    /* Делитель для 38400 бод: 115200 / 38400 = 3 */
    outb(PORT_DATA, 0x03);
    outb(PORT_IER,  0x00);

    /* LCR: 8 бит, без чётности, 1 стоп-бит (8N1), DLAB=0 */
    outb(PORT_LCR, 0x03);

    /* FIFO: включаем, чистим, порог 14 байт */
    outb(PORT_FIFO, 0xC7);

    /* MCR: DTR, RTS, OUT2 (OUT2 нужен для работы прерываний,
     * но мы в polling — оставим для совместимости) */
    outb(PORT_MCR, 0x0B);

    serial_ready = 1;
}

int serial_is_initialized(void) {
    return serial_ready;
}

/* Ждём, пока буфер передачи опустеет (бит 5 LSR). */
static void serial_wait_tx(void) {
    while ((inb(PORT_LSR) & 0x20) == 0) {
        /* spin */
    }
}

void serial_putchar(char c) {
    if (!serial_ready) return;

    /* '\n' → "\r\n" — многие терминалы требуют CR перед LF */
    if (c == '\n') {
        serial_wait_tx();
        outb(PORT_DATA, '\r');
    }
    serial_wait_tx();
    outb(PORT_DATA, (u8) c);
}

void serial_write(const char* str) {
    while (*str) {
        serial_putchar(*str++);
    }
}
