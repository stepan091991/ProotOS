#include "vga.h"
#include "kbd_buffer.h"

static u16*  vga_buffer = (u16*) VGA_ADDRESS;
static u32   vga_row    = 0;
static u32   vga_column = 0;
static u8    vga_color  = 0x07;

/* Pager state */
static int pager_pages  = 0;
static int pager_count  = 0;
static int pager_skip   = 0;

static inline u16 vga_entry(char c, u8 color) {
    return (u16) c | ((u16) color << 8);
}

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void vga_update_hardware_cursor(void) {
    u16 pos = (u16)(vga_row * VGA_WIDTH + vga_column);
    outb(0x3D4, 14);
    outb(0x3D5, (u8)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (u8)(pos & 0xFF));
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_color = (u8)((bg << 4) | (fg & 0x0F));
}

void vga_pager_set(int pages) {
    pager_pages = (pages > 0) ? pages : 0;
    pager_count = 0;
    pager_skip  = 0;
}

int vga_pager_get(void) {
    return pager_pages;
}

void vga_init(void) {
    vga_clear();
}

void vga_clear(void) {
    u16 blank = vga_entry(' ', vga_color);
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    vga_row = 0;
    vga_column = 0;
    vga_update_hardware_cursor();
}

void vga_get_cursor(u32* row, u32* col) {
    *row = vga_row;
    *col = vga_column;
}

void vga_set_cursor(u32 row, u32 col) {
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    if (col >= VGA_WIDTH)  col = VGA_WIDTH - 1;
    vga_row    = row;
    vga_column = col;
    vga_update_hardware_cursor();
}

/* ─── Pager prompt ─────────────────────────────────── */

static void pager_show_more(void) {
    /* Временно выключаем pager, чтобы наши же putchar'ы не рекурсировали. */
    int saved_pages = pager_pages;
    pager_pages = 0;

    /* Запоминаем, куда вернуться. */
    u32 save_row = vga_row;
    u32 save_col = vga_column;

    /* Рисуем приглашение. */
    const char* msg = "-- More --";
    for (const char* p = msg; *p; p++) {
        vga_putchar(*p);
    }

    /* Ждём нажатия. */
    for (;;) {
        int key = kbd_buffer_pop();
        if (key < 0) {
            __asm__ volatile ("hlt");
            continue;
        }
        if (key == 'q' || key == 'Q') {
            pager_skip = 1;
        }
        break;
    }

    /* Стираем приглашение. */
    vga_set_cursor(save_row, save_col);
    for (int i = 0; i < 10; i++) {
        vga_putchar(' ');
    }
    vga_set_cursor(save_row, save_col);

    pager_pages = saved_pages;
}

/* ─── Основной вывод ───────────────────────────────── */

void vga_putchar(char c) {
    if (pager_skip) return;

    if (c == '\n') {
        vga_column = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_column = 0;
    } else if (c == '\b') {
        if (vga_column > 0) vga_column--;
    } else if (c == '\t') {
        vga_column = (vga_column + 4) & ~3u;
    } else {
        vga_buffer[vga_row * VGA_WIDTH + vga_column] = vga_entry(c, vga_color);
        vga_column++;
    }

    if (vga_column >= VGA_WIDTH) {
        vga_column = 0;
        vga_row++;
    }

    if (vga_row >= VGA_HEIGHT) {
        for (u32 i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        u16 blank = vga_entry(' ', vga_color);
        for (u32 i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = blank;
        }
        vga_row = VGA_HEIGHT - 1;
    }

    vga_update_hardware_cursor();

    /* Постраничный вывод: считаем '\n' и, если пора, пауза. */
    if (pager_pages > 0 && c == '\n' && !pager_skip) {
        pager_count++;
        if (pager_count >= pager_pages) {
            pager_count = 0;
            pager_show_more();
        }
    }
}

void vga_print(const char* str) {
    while (*str) vga_putchar(*str++);
}
