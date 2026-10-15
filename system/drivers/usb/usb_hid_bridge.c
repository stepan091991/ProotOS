// system/drivers/usb/usb_hid_bridge.c
//
// Мост USB HID → kbd_buffer ProotOS.
//
// Использует GET_REPORT (control transfer) в цикле с process_sleep(20).
// Обрабатывает Shift (регистр и символы), Ctrl (управляющие коды),
// спец-клавиши (стрелки, Home/End/Del).

#include <stdint.h>
#include <stddef.h>

#include "usbh_core.h"
#include "usbh_hid.h"
#include "usb_config.h"
#include "usb_log.h"

#include "printf.h"
#include "string.h"
#include "kbd_buffer.h"

extern void process_sleep(uint32_t ms);
extern u32  process_create(const char *name, void (*fn)(void*), void *arg);

// ─── Модификаторы (byte 0 boot-отчёта) ───
#define MOD_LCTRL   0x01
#define MOD_LSHIFT  0x02
#define MOD_LALT    0x04
#define MOD_LGUI    0x08
#define MOD_RCTRL   0x10
#define MOD_RSHIFT  0x20
#define MOD_RALT    0x40
#define MOD_RGUI    0x80

#define HID_REPORT_TYPE_INPUT   0x01

static const char keymap_lo[128] = {
    [0x04] = 'a', [0x05] = 'b', [0x06] = 'c', [0x07] = 'd',
    [0x08] = 'e', [0x09] = 'f', [0x0A] = 'g', [0x0B] = 'h',
    [0x0C] = 'i', [0x0D] = 'j', [0x0E] = 'k', [0x0F] = 'l',
    [0x10] = 'm', [0x11] = 'n', [0x12] = 'o', [0x13] = 'p',
    [0x14] = 'q', [0x15] = 'r', [0x16] = 's', [0x17] = 't',
    [0x18] = 'u', [0x19] = 'v', [0x1A] = 'w', [0x1B] = 'x',
    [0x1C] = 'y', [0x1D] = 'z',
    [0x1E] = '1', [0x1F] = '2', [0x20] = '3', [0x21] = '4',
    [0x22] = '5', [0x23] = '6', [0x24] = '7', [0x25] = '8',
    [0x26] = '9', [0x27] = '0',
    [0x28] = '\n', [0x29] = 0x1B, [0x2A] = '\b', [0x2B] = '\t',
    [0x2C] = ' ', [0x2D] = '-', [0x2E] = '=', [0x2F] = '[',
    [0x30] = ']', [0x31] = '\\', [0x33] = ';', [0x34] = '\'',
    [0x35] = '`', [0x36] = ',', [0x37] = '.', [0x38] = '/',
};

static const char keymap_hi[128] = {
    [0x04] = 'A', [0x05] = 'B', [0x06] = 'C', [0x07] = 'D',
    [0x08] = 'E', [0x09] = 'F', [0x0A] = 'G', [0x0B] = 'H',
    [0x0C] = 'I', [0x0D] = 'J', [0x0E] = 'K', [0x0F] = 'L',
    [0x10] = 'M', [0x11] = 'N', [0x12] = 'O', [0x13] = 'P',
    [0x14] = 'Q', [0x15] = 'R', [0x16] = 'S', [0x17] = 'T',
    [0x18] = 'U', [0x19] = 'V', [0x1A] = 'W', [0x1B] = 'X',
    [0x1C] = 'Y', [0x1D] = 'Z',
    [0x1E] = '!', [0x1F] = '@', [0x20] = '#', [0x21] = '$',
    [0x22] = '%', [0x23] = '^', [0x24] = '&', [0x25] = '*',
    [0x26] = '(', [0x27] = ')',
    [0x28] = '\n', [0x29] = 0x1B, [0x2A] = '\b', [0x2B] = '\t',
    [0x2C] = ' ', [0x2D] = '_', [0x2E] = '+', [0x2F] = '{',
    [0x30] = '}', [0x31] = '|', [0x33] = ':', [0x34] = '"',
    [0x35] = '~', [0x36] = '<', [0x37] = '>', [0x38] = '?',
};

static int hid_special_key(uint8_t code)
{
    switch (code) {
        case 0x4A: return KBD_KEY_HOME;
        case 0x4C: return KBD_KEY_DEL;
        case 0x4D: return KBD_KEY_END;
        case 0x4F: return KBD_KEY_RIGHT;
        case 0x50: return KBD_KEY_LEFT;
        case 0x51: return KBD_KEY_DOWN;
        case 0x52: return KBD_KEY_UP;
        default:   return 0;
    }
}

static void hid_process_report(const uint8_t *cur, const uint8_t *prev)
{
    uint8_t mod   = cur[0];
    int shift     = (mod & (MOD_LSHIFT | MOD_RSHIFT)) ? 1 : 0;
    int ctrl      = (mod & (MOD_LCTRL  | MOD_RCTRL))  ? 1 : 0;

    for (int i = 2; i < 8; i++) {
        uint8_t code = cur[i];
        if (code == 0) continue;

        // Уже была нажата в предыдущем отчёте → не новое нажатие.
        int was = 0;
        for (int j = 2; j < 8; j++) {
            if (prev[j] == code) { was = 1; break; }
        }
        if (was) continue;

        // Спец-клавиши (стрелки, Home/End/Del) — независимо от модификаторов.
        int sp = hid_special_key(code);
        if (sp > 0) {
            kbd_buffer_push((u8)sp);
            continue;
        }

        // Ctrl: превращаем буквы и символы в управляющие коды.
        // Проверяем ДО Shift: Ctrl+A и Shift+Ctrl+A дают одинаковый 0x01.
        if (ctrl) {
            if (code >= 0x04 && code <= 0x1D) {
                // a..z → 0x01..0x1A (Ctrl+A..Ctrl+Z)
                kbd_buffer_push((u8)(code - 0x04 + 1));
                continue;
            }
            switch (code) {
                case 0x2C: kbd_buffer_push(0x00); continue;  // Ctrl+Space → NUL
                case 0x28: kbd_buffer_push('\n'); continue;  // Ctrl+Enter → LF
                case 0x2A: kbd_buffer_push(0x08); continue;  // Ctrl+Backspace → BS
                case 0x2F: kbd_buffer_push(0x1B); continue;  // Ctrl+[  → ESC
                case 0x31: kbd_buffer_push(0x1C); continue;  // Ctrl+\  → FS
                case 0x30: kbd_buffer_push(0x1D); continue;  // Ctrl+]  → GS
                case 0x34: kbd_buffer_push(0x1C); continue;  // Ctrl+'  → FS (иногда)
                case 0x38: kbd_buffer_push(0x1F); continue;  // Ctrl+/  → US
                default: break;
            }
            // Прочие Ctrl-комбинации игнорируем.
            continue;
        }

        // Обычные символы (с учётом Shift).
        char c = 0;
        if (shift) { if (code < 128) c = keymap_hi[code]; }
        else       { if (code < 128) c = keymap_lo[code]; }
        if (c) kbd_buffer_push((u8)c);
    }
}

// ─── Фоновая нить HID: GET_REPORT polling ───

static struct usbh_hid *g_hid = NULL;

static void usb_hid_thread(void *arg)
{
    (void)arg;
    struct usbh_hid *hid = g_hid;
    if (!hid) return;

    printf("[usb-hid] poll loop started\n");

    uint8_t report[16];
    uint8_t prev[16];
    memset(prev, 0, sizeof(prev));

    int err_count = 0;
    for (;;) {
        memset(report, 0, sizeof(report));
        int ret = usbh_hid_get_report(hid, HID_REPORT_TYPE_INPUT, 0,
                                      report, 8);
        if (ret >= 0) {
            hid_process_report(report, prev);
            memcpy(prev, report, 8);
            err_count = 0;
        } else {
            err_count++;
            if (err_count <= 3 || (err_count % 100) == 0) {
                printf("[usb-hid] GET_REPORT ret=%d (err #%d)\n", ret, err_count);
            }
        }
        process_sleep(20);
    }
}

void usbh_hid_run(struct usbh_hid *hid_class)
{
    if (g_hid != NULL) return;
    g_hid = hid_class;

    printf("[usb-hid] HID registered\n");

    int r = usbh_hid_set_protocol(hid_class, 0);
    printf("[usb-hid] set_protocol(BOOT) = %d\n", r);

    process_create("usb_hid", usb_hid_thread, NULL);
}

void usb_hid_bridge_init(void)
{
    // Ничего не делаем: usbh_hid_run вызовется сам при connect.
}
