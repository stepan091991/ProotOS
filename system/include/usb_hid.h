#ifndef PROOTOS_USB_HID_H
#define PROOTOS_USB_HID_H

#include "types.h"

/* Заглушки — HID не реализован, но shell.c их упоминает. */
static inline int  usb_hid_init_on_addr(u8 addr) { (void)addr; return 0; }
static inline void usb_hid_poll(void) { }
static inline int  usb_hid_is_active(void) { return 0; }

#endif