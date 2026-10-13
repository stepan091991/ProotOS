#include "banner.h"
#include "vga.h"
#include "printf.h"

void print_prootos_banner(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("  ____  ____   ___   ___  _____  ___  ____  \n");
    vga_print(" |  _ \\|  _ \\ / _ \\ / _ \\|_   _|/ _ \\/ ___| \n");
    vga_print(" | |_) | |_) | | | | | | | | | | | | \\___ \\ \n");
    vga_print(" |  __/|  _ <| |_| | |_| | | | | |_| |___) |\n");
    vga_print(" |_|   |_| \\_\\\\___/ \\___/  |_|  \\___/|____/ \n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}
