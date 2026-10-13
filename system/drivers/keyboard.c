#include "keyboard.h"
#include "isr.h"
#include "pic.h"
#include "printf.h"
#include "vga.h"
#include "kbd_buffer.h"

#define KBD_DATA_PORT   0x60

static const char kbd_us_lower[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',  0,   ' ', 0,
};

static const char kbd_us_upper[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*',  0,   ' ', 0,
};

static int shift_pressed = 0;
static int ctrl_pressed  = 0;
static int extended_prefix = 0;

#define KBD_LEFT_SHIFT   0x2A
#define KBD_RIGHT_SHIFT  0x36
#define KBD_LEFT_CTRL    0x1D

static void keyboard_callback(registers_t* regs) {
    (void) regs;

    u8 scancode;
    __asm__ volatile ("inb %1, %0" : "=a"(scancode) : "Nd"(KBD_DATA_PORT));

    /* Расширенный префикс: 0xE0 + второй байт. */
    if (extended_prefix) {
        extended_prefix = 0;

        if (!(scancode & 0x80)) {
            switch (scancode) {
                case 0x48: kbd_buffer_push(KBD_KEY_UP);    break;
                case 0x50: kbd_buffer_push(KBD_KEY_DOWN);  break;
                case 0x4B: kbd_buffer_push(KBD_KEY_LEFT);  break;
                case 0x4D: kbd_buffer_push(KBD_KEY_RIGHT); break;
                case 0x47: kbd_buffer_push(KBD_KEY_HOME);  break;
                case 0x4F: kbd_buffer_push(KBD_KEY_END);   break;
                case 0x53: kbd_buffer_push(KBD_KEY_DEL);   break;

                /* Numpad Enter = E0 1C */
                case 0x1C: kbd_buffer_push('\n');          break;

                /* Numpad / = E0 35 */
                case 0x35: kbd_buffer_push('/');           break;

                default: break;
            }
        }
        pic_send_eoi(1);
        return;
    }

    if (scancode == 0xE0) {
        extended_prefix = 1;
        pic_send_eoi(1);
        return;
    }

    if (scancode & 0x80) {
        u8 released = scancode & 0x7F;
        if (released == KBD_LEFT_SHIFT || released == KBD_RIGHT_SHIFT) {
            shift_pressed = 0;
        } else if (released == KBD_LEFT_CTRL) {
            ctrl_pressed = 0;
        }
    } else {
        if (scancode == KBD_LEFT_SHIFT || scancode == KBD_RIGHT_SHIFT) {
            shift_pressed = 1;
        } else if (scancode == KBD_LEFT_CTRL) {
            ctrl_pressed = 1;
        } else if (scancode < 128) {
            char c = shift_pressed ? kbd_us_upper[scancode]
                                   : kbd_us_lower[scancode];
            if (c) {
                if (ctrl_pressed && c >= 'a' && c <= 'z') {
                    c = c - 'a' + 1;
                }
                kbd_buffer_push((u8) c);
            }
        }
    }

    pic_send_eoi(1);
}

void keyboard_init(void) {
    kbd_buffer_init();
    isr_register_handler(33, keyboard_callback);
    pic_clear_mask(1);
    printf("[kbd] initialized, IRQ1 unmasked\n");
}