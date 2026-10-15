// system/drivers/usb/usb_hc_low_level.c

#include <stdint.h>

#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_config.h"
#include "usb_log.h"
#include "usb_hc_ehci.h"

#include "pci.h"
#include "pic.h"
#include "isr.h"
#include "printf.h"

extern void process_sleep(uint32_t ms);
extern u32  process_create(const char *name, void (*fn)(void*), void *arg);
extern void USBH_IRQHandler(uint8_t busid);

extern void usb_hid_bridge_init(void);
extern void usb_msc_bridge_init(void);

void usb_hc_low_level_init(struct usbh_bus *bus);
void usb_hc_low_level_deinit(struct usbh_bus *bus);

static int g_usb_irq = -1;
volatile u32 g_usb_irq_count = 0;

static void usb_irq_dispatch(registers_t *regs)
{
    (void)regs;
    g_usb_irq_count++;
    if (g_usb_irq_count <= 3) {
        printf("[usb] IRQ #%u\n", g_usb_irq_count);
    }
    USBH_IRQHandler(0);
    if (g_usb_irq >= 0) pic_send_eoi((u8)g_usb_irq);
}

// Polling-нить. ВАЖНО: интервал должен быть МЕНЬШЕ, чем интервал
// interrupt-endpoint клавиатуры (у нас bInterval=7 → 8 мс для HS).
// Иначе URB завершаются, но callback не вызывается вовремя.
static void usb_poll_thread(void *arg)
{
    (void)arg;

    process_sleep(50);   // 500 мс — ждём enumeration

    {
        struct usbh_bus *bus = &g_usbhost_bus[0];
        uint32_t base = bus->hcd.reg_base
        + CONFIG_USB_EHCI_HCCR_OFFSET
        + g_ehci_hcd[0].hcor_offset;

        uint8_t nports = g_ehci_hcd[0].n_ports;
        printf("[usb] probing %u EHCI ports...\n", nports);

        for (uint8_t p = 0; p < nports && p < CONFIG_USBHOST_MAX_RHPORTS; p++) {
            volatile uint32_t *portsc = (volatile uint32_t *)(base + 0x44 + 4 * p);
            uint32_t v = *portsc;
            printf("[usb] port %u: PORTSC=%08x\n", p, (unsigned)v);
            if (v & (1u << 0)) {
                bus->hcd.roothub.int_buffer[0] |= (1u << (p + 1));
            }
        }

        usbh_hub_thread_wakeup(&bus->hcd.roothub);
        printf("[usb] hub thread woken up\n");
    }

    // 10 мс — компромисс: чаще чем bInterval клавиатуры (8 мс),
    // но не сжигает CPU на 100%.
    for (;;) {
        USBH_IRQHandler(0);
        process_sleep(10);
    }
}

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    (void)bus;

    pci_device_t *dev = NULL;
    int count = pci_device_count();
    for (int i = 0; i < count; i++) {
        pci_device_t *d = pci_get_device(i);
        if (d->class_code == 0x0C && d->subclass == 0x03 && d->prog_if == 0x20) {
            dev = d;
            break;
        }
    }
    if (!dev) {
        USB_LOG_ERR("usb: EHCI not found on PCI\r\n");
        return;
    }

    g_usb_irq = dev->irq_line;
    isr_register_handler(32 + (u8)g_usb_irq, usb_irq_dispatch);
    pic_clear_mask((u8)g_usb_irq);

    printf("[usb] IRQ %d registered\n", g_usb_irq);

    process_create("usb_poll", usb_poll_thread, NULL);
    usb_hid_bridge_init();
    usb_msc_bridge_init();
}

void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    (void)bus;
    if (g_usb_irq >= 0) {
        pic_set_mask((u8)g_usb_irq);
        g_usb_irq = -1;
    }
}

uint8_t usbh_get_port_speed(struct usbh_bus *bus, const uint8_t port)
{
    (void)bus;
    (void)port;
    return USB_SPEED_HIGH;
}
