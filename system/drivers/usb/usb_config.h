// system/drivers/usb/usb_config.h
#ifndef CHERRYUSB_CONFIG_H
#define CHERRYUSB_CONFIG_H

// ─────────────────────────────────────────────────────────
// Вывод отладочной информации
// ─────────────────────────────────────────────────────────
#include "printf.h"
#define CONFIG_USB_PRINTF(...)   printf(__VA_ARGS__)

#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL     USB_DBG_INFO
#endif

// ─────────────────────────────────────────────────────────
// Выравнивание дескрипторов (EHCI требует 32 байта)
// ─────────────────────────────────────────────────────────
#define CONFIG_USB_ALIGN_SIZE    32

// USB_NOCACHE_RAM_SECTION переопределяем в пустоту.
// На i386 в QEMU DMA когерентен, на реальном железе —
// отдельная задача через MTRR. Без этого переопределения
// линковщик будет искать секцию .noncacheable.
#define USB_NOCACHE_RAM_SECTION

// ─────────────────────────────────────────────────────────
// Общие параметры host-стека
// ─────────────────────────────────────────────────────────
#ifndef CONFIG_USBHOST_MAX_BUS
#define CONFIG_USBHOST_MAX_BUS              1
#endif

#ifndef CONFIG_USBHOST_MAX_RHPORTS
#define CONFIG_USBHOST_MAX_RHPORTS          8
#endif

#ifndef CONFIG_USBHOST_MAX_EXTHUBS
#define CONFIG_USBHOST_MAX_EXTHUBS          1
#endif

#ifndef CONFIG_USBHOST_MAX_EHPORTS
#define CONFIG_USBHOST_MAX_EHPORTS          4
#endif

#ifndef CONFIG_USBHOST_MAX_INTERFACES
#define CONFIG_USBHOST_MAX_INTERFACES       8
#endif

#ifndef CONFIG_USBHOST_MAX_INTF_ALTSETTINGS
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 2
#endif

#ifndef CONFIG_USBHOST_MAX_ENDPOINTS
#define CONFIG_USBHOST_MAX_ENDPOINTS        8
#endif

#define CONFIG_USBHOST_DEV_NAMELEN          16

// ─────────────────────────────────────────────────────────
// Классы: включаем только HID и MSC
// ─────────────────────────────────────────────────────────
#define CONFIG_USBHOST_MAX_SERIAL_CLASS     1
#define CONFIG_USBHOST_MAX_HID_CLASS        2
#define CONFIG_USBHOST_MAX_MSC_CLASS        1
#define CONFIG_USBHOST_MAX_AUDIO_CLASS      1
#define CONFIG_USBHOST_MAX_VIDEO_CLASS      1

// ─────────────────────────────────────────────────────────
// Таймауты и размеры буферов
// ─────────────────────────────────────────────────────────
#ifndef CONFIG_USBHOST_REQUEST_BUFFER_LEN
#define CONFIG_USBHOST_REQUEST_BUFFER_LEN       512
#endif

#ifndef CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT
#define CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT 500
#endif

#ifndef CONFIG_USBHOST_MSC_TIMEOUT
#define CONFIG_USBHOST_MSC_TIMEOUT              5000
#endif

// ─────────────────────────────────────────────────────────
// EHCI-параметры
// ─────────────────────────────────────────────────────────
#define CONFIG_USB_EHCI_HCCR_OFFSET     (0x0)
#define CONFIG_USB_EHCI_FRAME_LIST_SIZE 1024
#define CONFIG_USB_EHCI_QH_NUM          10
#define CONFIG_USB_EHCI_QTD_NUM         (CONFIG_USB_EHCI_QH_NUM * 3)
#define CONFIG_USB_EHCI_ITD_NUM         4

// ─────────────────────────────────────────────────────────
// OHCI (не используется, но нужен для заголовков)
// ─────────────────────────────────────────────────────────
#define CONFIG_USB_OHCI_HCOR_OFFSET (0x0)
#define CONFIG_USB_OHCI_ED_NUM      10
#define CONFIG_USB_OHCI_TD_NUM      3

// ─────────────────────────────────────────────────────────
// Прочее
// ─────────────────────────────────────────────────────────
#ifndef CONFIG_USBHOST_PSC_PRIO
#define CONFIG_USBHOST_PSC_PRIO     0
#endif
#ifndef CONFIG_USBHOST_PSC_STACKSIZE
#define CONFIG_USBHOST_PSC_STACKSIZE 4096
#endif

#ifndef usb_phyaddr2ramaddr
#define usb_phyaddr2ramaddr(addr) (addr)
#endif
#ifndef usb_ramaddr2phyaddr
#define usb_ramaddr2phyaddr(addr) (addr)
#endif

// Отключаем встроенный usb_memcpy (там ARM-специфичные хаки).
// На i386 используем свой memcpy из libc_shim.c.
#define CONFIG_USB_MEMCPY_DISABLE

#endif // CHERRYUSB_CONFIG_H
