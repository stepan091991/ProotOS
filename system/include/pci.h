#ifndef PROOTOS_PCI_H
#define PROOTOS_PCI_H

#include "types.h"

typedef struct {
    u8   bus;
    u8   slot;
    u8   func;
    u16  vendor_id;
    u16  device_id;
    u8   class_code;
    u8   subclass;
    u8   prog_if;
    u8   revision;
    u8   header_type;
    u32  bar[6];
    u8   irq_line;
    u8   irq_pin;
} pci_device_t;

#define PCI_MAX_DEVICES 64

#define PCI_CLASS_STORAGE   0x01
#define PCI_CLASS_NETWORK   0x02
#define PCI_CLASS_DISPLAY   0x03
#define PCI_CLASS_SERIAL    0x0C

#define PCI_SUBCLASS_USB_UHCI   0x00
#define PCI_SUBCLASS_USB_OHCI   0x10
#define PCI_SUBCLASS_USB_EHCI   0x20
#define PCI_SUBCLASS_USB_XHCI   0x30

void pci_init(void);
void pci_dump(void);

int pci_device_count(void);
pci_device_t* pci_get_device(int index);
pci_device_t* pci_find(u16 vendor, u16 device,
                       u8 class_code, u8 subclass);

u32  pci_cfg_read32(pci_device_t* d, u8 offset);
u16  pci_cfg_read16(pci_device_t* d, u8 offset);
u8   pci_cfg_read8 (pci_device_t* d, u8 offset);
void pci_cfg_write32(pci_device_t* d, u8 offset, u32 value);
void pci_cfg_write16(pci_device_t* d, u8 offset, u16 value);

void pci_enable_io(pci_device_t* d);
void pci_enable_mmio(pci_device_t* d);
void pci_enable_bus_master(pci_device_t* d);

u32 pci_bar_mmio(pci_device_t* d, int bar_index);
u32 pci_bar_io(pci_device_t* d, int bar_index);

#endif
