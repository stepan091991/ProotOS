#include "pci.h"
#include "printf.h"
#include "string.h"

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

#define PCI_CMD_IO        (1 << 0)
#define PCI_CMD_MEM       (1 << 1)
#define PCI_CMD_BUSMASTER (1 << 2)

static pci_device_t devices[PCI_MAX_DEVICES];
static int device_count = 0;

static inline void outl(u16 port, u32 value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}
static inline u32 inl(u16 port) {
    u32 v;
    __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static u32 pci_read32_raw(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 addr = 0x80000000u
    | ((u32) bus  << 16)
    | ((u32) slot << 11)
    | ((u32) func <<  8)
    | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write32_raw(u8 bus, u8 slot, u8 func, u8 offset, u32 value) {
    u32 addr = 0x80000000u
    | ((u32) bus  << 16)
    | ((u32) slot << 11)
    | ((u32) func <<  8)
    | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, value);
}

static u16 pci_read16_raw(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 v = pci_read32_raw(bus, slot, func, offset);
    return (u16)((v >> ((offset & 2) * 8)) & 0xFFFF);
}
static u8 pci_read8_raw(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 v = pci_read32_raw(bus, slot, func, offset);
    return (u8)((v >> ((offset & 3) * 8)) & 0xFF);
}

u32 pci_cfg_read32(pci_device_t* d, u8 offset) {
    return pci_read32_raw(d->bus, d->slot, d->func, offset);
}
u16 pci_cfg_read16(pci_device_t* d, u8 offset) {
    return pci_read16_raw(d->bus, d->slot, d->func, offset);
}
u8 pci_cfg_read8(pci_device_t* d, u8 offset) {
    return pci_read8_raw(d->bus, d->slot, d->func, offset);
}
void pci_cfg_write32(pci_device_t* d, u8 offset, u32 value) {
    pci_write32_raw(d->bus, d->slot, d->func, offset, value);
}
void pci_cfg_write16(pci_device_t* d, u8 offset, u16 value) {
    u32 old = pci_cfg_read32(d, offset & ~3);
    u32 shift = (offset & 2) * 8;
    u32 mask  = 0xFFFFu << shift;
    u32 newv  = (old & ~mask) | ((u32) value << shift);
    pci_cfg_write32(d, offset & ~3, newv);
}

void pci_enable_io(pci_device_t* d) {
    u16 cmd = pci_cfg_read16(d, 0x04);
    cmd |= PCI_CMD_IO;
    pci_cfg_write16(d, 0x04, cmd);
}
void pci_enable_mmio(pci_device_t* d) {
    u16 cmd = pci_cfg_read16(d, 0x04);
    cmd |= PCI_CMD_MEM;
    pci_cfg_write16(d, 0x04, cmd);
}
void pci_enable_bus_master(pci_device_t* d) {
    u16 cmd = pci_cfg_read16(d, 0x04);
    cmd |= PCI_CMD_BUSMASTER;
    pci_cfg_write16(d, 0x04, cmd);
}

u32 pci_bar_mmio(pci_device_t* d, int bar_index) {
    if (bar_index < 0 || bar_index > 5) return 0;
    u32 bar = d->bar[bar_index];
    if (bar & 0x01) return 0;
    return bar & 0xFFFFFFF0;
}
u32 pci_bar_io(pci_device_t* d, int bar_index) {
    if (bar_index < 0 || bar_index > 5) return 0;
    u32 bar = d->bar[bar_index];
    if (!(bar & 0x01)) return 0;
    return bar & 0xFFFFFFFC;
}

static void pci_scan_device(u8 bus, u8 slot, u8 func) {
    if (device_count >= PCI_MAX_DEVICES) return;

    u16 vendor = pci_read16_raw(bus, slot, func, 0x00);
    if (vendor == 0xFFFF) return;

    pci_device_t* d = &devices[device_count++];
    memset(d, 0, sizeof(*d));

    d->bus        = bus;
    d->slot       = slot;
    d->func       = func;
    d->vendor_id  = vendor;
    d->device_id  = pci_read16_raw(bus, slot, func, 0x02);
    d->revision   = pci_read8_raw (bus, slot, func, 0x08);
    d->prog_if    = pci_read8_raw (bus, slot, func, 0x09);
    d->subclass   = pci_read8_raw (bus, slot, func, 0x0A);
    d->class_code = pci_read8_raw (bus, slot, func, 0x0B);
    d->header_type = pci_read8_raw(bus, slot, func, 0x0E) & 0x7F;

    if (d->header_type == 0x00) {
        for (int i = 0; i < 6; i++) {
            d->bar[i] = pci_read32_raw(bus, slot, func, 0x10 + i * 4);
        }
    }
    d->irq_line = pci_read8_raw(bus, slot, func, 0x3C);
    d->irq_pin  = pci_read8_raw(bus, slot, func, 0x3D);
}

static void pci_scan_bus(u8 bus) {
    for (u8 slot = 0; slot < 32; slot++) {
        u16 vendor = pci_read16_raw(bus, slot, 0, 0x00);
        if (vendor == 0xFFFF) continue;

        pci_scan_device(bus, slot, 0);

        u8 header = pci_read8_raw(bus, slot, 0, 0x0E);
        if (header & 0x80) {
            for (u8 func = 1; func < 8; func++) {
                u16 v = pci_read16_raw(bus, slot, func, 0x00);
                if (v != 0xFFFF) pci_scan_device(bus, slot, func);
            }
        }
    }
}

void pci_init(void) {
    device_count = 0;
    memset(devices, 0, sizeof(devices));

    u16 test = pci_read16_raw(0, 0, 0, 0x00);
    if (test == 0xFFFF) {
        printf("[pci] no PCI bus detected\n");
        return;
    }
    pci_scan_bus(0);
    printf("[pci] found %u device(s)\n", device_count);
}

int pci_device_count(void) { return device_count; }
pci_device_t* pci_get_device(int index) {
    if (index < 0 || index >= device_count) return NULL;
    return &devices[index];
}
pci_device_t* pci_find(u16 vendor, u16 device, u8 class_code, u8 subclass) {
    for (int i = 0; i < device_count; i++) {
        pci_device_t* d = &devices[i];
        if (vendor != 0xFFFF && d->vendor_id != vendor) continue;
        if (device    != 0xFFFF && d->device_id != device) continue;
        if (class_code != 0xFF && d->class_code != class_code) continue;
        if (subclass   != 0xFF && d->subclass   != subclass)   continue;
        return d;
    }
    return NULL;
}

static const char* class_name(u8 c, u8 sc) {
    if (c == 0x01) {
        if (sc == 0x01) return "IDE";
        if (sc == 0x06) return "SATA";
        if (sc == 0x08) return "NVMe";
        return "Storage";
    }
    if (c == 0x02) return "Network";
    if (c == 0x03) return "Display";
    if (c == 0x06 && sc == 0x00) return "Host bridge";
    if (c == 0x06 && sc == 0x01) return "ISA bridge";
    if (c == 0x06 && sc == 0x04) return "PCI bridge";
    if (c == 0x0C && sc == 0x03) return "USB";
    if (c == 0x0C) return "Serial";
    return "?";
}

static const char* usb_type(u8 prog_if) {
    switch (prog_if) {
        case 0x00: return "UHCI";
        case 0x10: return "OHCI";
        case 0x20: return "EHCI";
        case 0x30: return "xHCI";
        default:   return "USB?";
    }
}

void pci_dump(void) {
    printf("[pci] %d device(s):\n", device_count);
    for (int i = 0; i < device_count; i++) {
        pci_device_t* d = &devices[i];
        printf("  %02x:%02x.%x  %04x:%04x  %02x.%02x  %-12s",
               d->bus, d->slot, d->func,
               d->vendor_id, d->device_id,
               d->class_code, d->subclass,
               class_name(d->class_code, d->subclass));

        if (d->class_code == 0x0C && d->subclass == 0x03) {
            printf(" %s", usb_type(d->prog_if));
        }
        if (d->header_type == 0) {
            for (int b = 0; b < 6; b++) {
                if ((d->bar[b] & 0x01) == 0 && d->bar[b] != 0) {
                    printf("  MMIO@%p", (void*)(d->bar[b] & 0xFFFFFFF0));
                    break;
                }
            }
            printf("  IRQ=%u", d->irq_line);
        }
        printf("\n");
    }
}
