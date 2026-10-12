#include "panic.h"
#include "vga.h"
#include "string.h"
#include "kernel.h"

static void print_u32_dec(u32 value) {
    char buf[11];   /* 10 цифр + '\0' */
    int  i = 0;

    if (value == 0) {
        vga_putchar('0');
        return;
    }

    while (value > 0 && i < 10) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    buf[i] = '\0';

    strrev(buf);
    vga_print(buf);
}

void panic_at(const char* file, int line, const char* message) {
    /* Мы уже не вернёмся — прерывания нам больше не нужны. */
    __asm__ volatile ("cli");

    vga_set_color(VGA_WHITE, VGA_RED);
    vga_print("\n\n *** KERNEL PANIC *** \n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("\n");

    vga_print("  Message: ");
    vga_print(message);
    vga_putchar('\n');

    vga_print("  At:      ");
    vga_print(file);
    vga_putchar(':');
    print_u32_dec((u32) line);
    vga_putchar('\n');

    vga_print("\n  The system has been halted.\n");

    halt();
}

void panic(const char* message) {
    panic_at("(unknown)", 0, message);
}
