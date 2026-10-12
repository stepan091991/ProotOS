#include "uhci.h"
#include "pci.h"
#include "printf.h"
#include "string.h"
#include "pmm.h"

/* ─── I/O ──────────────────────────────────────────── */

static inline void outw(u16 p, u16 v) { __asm__ volatile ("outw %0, %1" : : "a"(v), "Nd"(p)); }
static inline u16  inw (u16 p) { u16 v; __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outl(u16 p, u32 v) { __asm__ volatile ("outl %0, %1" : : "a"(v), "Nd"(p)); }
static inline u32  inl (u16 p) { u32 v; __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }

/* ─── Регистры ─────────────────────────────────────── */

#define UHCI_USBCMD     0x00
#define UHCI_USBSTS     0x02
#define UHCI_USBINTR    0x04
#define UHCI_FRNUM      0x06
#define UHCI_FRBASEADD  0x08
#define UHCI_SOFMOD     0x0C
#define UHCI_PORTSC1    0x10
#define UHCI_PORTSC2    0x12

#define USBCMD_RS       0x0001
#define USBCMD_HCRESET  0x0002
#define USBCMD_GRESET   0x0004
#define USBCMD_CF       0x0040

#define USBSTS_HCH      0x0020

#define PORTSC_CCS      0x0001
#define PORTSC_CSC      0x0002
#define PORTSC_PED      0x0004
#define PORTSC_PEDC     0x0008
#define PORTSC_LSDA     0x0100
#define PORTSC_PR       0x0200

/* ─── QH/TD ────────────────────────────────────────── */

typedef struct {
    u32 qhlp;
    u32 qhel;
    u32 reserved[2];
} __attribute__((aligned(16))) uhci_qh_t;

typedef struct {
    u32 link;
    u32 status;
    u32 token;
    u32 buffer[5];
    u32 sw[4];
} __attribute__((aligned(16))) uhci_td_t;

#define TD_LINK_TERM    0x00000001
#define TD_LINK_QH      0x00000002
#define TD_LINK_VF      0x00000004

/* TD status — правильные биты */
#define TD_ST_LS        (1 << 26)
#define TD_ST_IOC       (1 << 24)
#define TD_ST_ACTIVE    (1 << 23)
#define TD_ST_STALLED   (1 << 22)
#define TD_ST_DBERR     (1 << 21)
#define TD_ST_BABBLE    (1 << 20)
#define TD_ST_NAK       (1 << 19)
#define TD_ST_CRC       (1 << 18)
#define TD_ST_BITSTUFF  (1 << 17)
#define TD_ST_ERRMASK   (TD_ST_STALLED | TD_ST_DBERR | TD_ST_BABBLE | \
TD_ST_CRC | TD_ST_BITSTUFF)

/* TD token */
#define TD_PID_SETUP    0x2D
#define TD_PID_IN       0x69
#define TD_PID_OUT      0xE1
#define TD_TOK_DEVADDR_SHIFT   8
#define TD_TOK_ENDPT_SHIFT     15
#define TD_TOK_TOGGLE          (1 << 19)
#define TD_TOK_MAXLEN_SHIFT    21

/* ─── Состояние ────────────────────────────────────── */

static u16 io_base   = 0;
static int uhci_ok   = 0;
static int num_ports = 0;

static u32* frame_list = NULL;
static uhci_qh_t idle_qh __attribute__((aligned(16)));

static uhci_td_t ctrl_td[8] __attribute__((aligned(16)));
static u8        ctrl_data[64] __attribute__((aligned(16)));

/* ─── Хелперы ──────────────────────────────────────── */

static u16 rd_w(u16 o) { return inw(io_base + o); }
static void wr_w(u16 o, u16 v) { outw(io_base + o, v); }
static void wr_l(u16 o, u32 v) { outl(io_base + o, v); }

/* ─── Init ─────────────────────────────────────────── */

int uhci_init(void) {
    pci_device_t* dev = NULL;
    for (int i = 0; i < pci_device_count(); i++) {
        pci_device_t* d = pci_get_device(i);
        if (d->class_code == PCI_CLASS_SERIAL &&
            d->subclass   == 0x03 &&
            d->prog_if    == PCI_SUBCLASS_USB_UHCI) {
            dev = d;
        break;
            }
    }
    if (!dev) {
        printf("[uhci] no UHCI controller\n");
        return 0;
    }

    printf("[uhci] controller at %02x:%02x.%x (%04x:%04x)\n",
           dev->bus, dev->slot, dev->func, dev->vendor_id, dev->device_id);

    pci_enable_io(dev);
    pci_enable_bus_master(dev);

    u32 bar4 = pci_cfg_read32(dev, 0x20);
    if (!(bar4 & 0x01)) {
        printf("[uhci] BAR4 not I/O (0x%x)\n", bar4);
        return 0;
    }
    io_base = (u16)(bar4 & 0xFFFC);
    printf("[uhci] I/O base = 0x%04x\n", io_base);

    if (rd_w(UHCI_USBCMD) == 0xFFFF) {
        printf("[uhci] no response\n");
        return 0;
    }

    wr_w(UHCI_USBCMD, USBCMD_GRESET);
    for (volatile int i = 0; i < 100000; i++) { }
    wr_w(UHCI_USBCMD, 0);

    wr_w(UHCI_USBCMD, USBCMD_HCRESET);
    for (int i = 0; i < 1000; i++) {
        if (!(rd_w(UHCI_USBCMD) & USBCMD_HCRESET)) break;
    }
    wr_w(UHCI_USBCMD, 0);

    frame_list = (u32*) pmm_alloc_page();
    if (!frame_list) {
        printf("[uhci] no memory for frame list\n");
        return 0;
    }

    memset(&idle_qh, 0, sizeof(idle_qh));
    idle_qh.qhlp = TD_LINK_TERM;
    idle_qh.qhel = TD_LINK_TERM;

    u32 idle_ptr = ((u32)&idle_qh) | TD_LINK_QH;
    for (int i = 0; i < 1024; i++) {
        frame_list[i] = idle_ptr;
    }

    wr_w(UHCI_USBINTR, 0);
    wr_l(UHCI_FRBASEADD, (u32) frame_list);
    wr_w(UHCI_FRNUM, 0);
    wr_w(UHCI_USBCMD, USBCMD_RS | USBCMD_CF);

    for (int i = 0; i < 1000; i++) {
        if (!(rd_w(UHCI_USBSTS) & USBSTS_HCH)) break;
    }
    if (rd_w(UHCI_USBSTS) & USBSTS_HCH) {
        printf("[uhci] controller didn't start\n");
        return 0;
    }

    num_ports = 2;
    printf("[uhci] initialized, %d port(s), frame_list=%p, idle_qh=%p\n",
           num_ports, (void*) frame_list, (void*) &idle_qh);

    uhci_ok = 1;
    return 1;
}

/* ─── Порты ────────────────────────────────────────── */

int uhci_port_connected(int port) {
    if (!uhci_ok || port < 1 || port > num_ports) return 0;
    u16 off = (port == 1) ? UHCI_PORTSC1 : UHCI_PORTSC2;
    return (rd_w(off) & PORTSC_CCS) ? 1 : 0;
}

int uhci_port_reset(int port) {
    if (!uhci_ok || port < 1 || port > num_ports) return 0;
    u16 off = (port == 1) ? UHCI_PORTSC1 : UHCI_PORTSC2;

    u16 sc0 = rd_w(off);
    if (!(sc0 & PORTSC_CCS)) return 0;

    wr_w(off, sc0);
    for (volatile int i = 0; i < 2000000; i++) { }

    u16 sc1 = rd_w(off);
    wr_w(off, (sc1 & ~(PORTSC_PED | PORTSC_PEDC | PORTSC_PR | PORTSC_CSC))
    | PORTSC_PR);
    for (volatile int i = 0; i < 20000000; i++) { }

    u16 sc2 = rd_w(off);
    wr_w(off, sc2 & ~(PORTSC_PED | PORTSC_PEDC | PORTSC_PR | PORTSC_CSC));
    for (volatile int i = 0; i < 20000000; i++) { }

    u16 sc3 = rd_w(off);
    if (sc3 & PORTSC_PED) return 1;

    wr_w(off, (sc3 & ~(PORTSC_PEDC | PORTSC_CSC | PORTSC_PR)) | PORTSC_PED);
    for (volatile int i = 0; i < 5000000; i++) { }

    return (rd_w(off) & PORTSC_PED) ? 1 : 0;
}

void uhci_dump(void) {
    if (!uhci_ok) { printf("[uhci] not initialized\n"); return; }
    printf("[uhci] io_base=0x%04x ports=%d\n", io_base, num_ports);
    printf("[uhci] USBCMD=0x%04x USBSTS=0x%04x FRNUM=%u\n",
           rd_w(UHCI_USBCMD), rd_w(UHCI_USBSTS), rd_w(UHCI_FRNUM));

    for (int p = 1; p <= num_ports; p++) {
        u16 off = (p == 1) ? UHCI_PORTSC1 : UHCI_PORTSC2;
        u16 sc  = rd_w(off);
        printf("[uhci] port %d: PORTSC=0x%04x %s%s%s%s\n",
               p, sc,
               (sc & PORTSC_CCS)  ? "CCS " : "",
               (sc & PORTSC_PED)  ? "PED " : "",
               (sc & PORTSC_LSDA) ? "LS "  : "",
               (sc & PORTSC_PR)   ? "RESET " : "");
    }
}

/* ─── Control transfer ─────────────────────────────── */

static void fill_td(uhci_td_t* td, u32 pid, u8 addr, u8 ep,
                    int toggle, u32 max_len, u32 phys, int ioc)
{
    memset(td, 0, sizeof(*td));
    td->link = TD_LINK_TERM;

    u32 status = TD_ST_ACTIVE;
    if (ioc) status |= TD_ST_IOC;
    td->status = status;

    u32 token = pid
    | ((u32) addr << TD_TOK_DEVADDR_SHIFT)
    | ((u32) ep   << TD_TOK_ENDPT_SHIFT)
    | (((max_len > 0 ? max_len : 1) - 1) << TD_TOK_MAXLEN_SHIFT);
    if (toggle) token |= TD_TOK_TOGGLE;
    td->token = token;

    td->buffer[0] = phys;
}
int uhci_control_transfer(u8 addr, int low_speed, u8 max_packet,
                          const usb_setup_packet_t* setup,
                          void* data, u32 data_len, int data_in)
{
    if (!uhci_ok) return 0;
    if (max_packet == 0) max_packet = 8;

    /* ─── Setup stage: 1 TD, 8 байт. ─────────────────── */
    memcpy(ctrl_data, setup, 8);
    fill_td(&ctrl_td[0], TD_PID_SETUP, addr, 0, 0, 8,
            (u32) ctrl_data, 0);
    if (low_speed) ctrl_td[0].status |= TD_ST_LS;

    uhci_td_t* prev = &ctrl_td[0];
    int idx = 1;

    /* ─── Data stage: разбиваем на TD по max_packet. ── */
    if (data_len > 0 && data != NULL) {
        u32 remaining = data_len;
        u8* ptr = (u8*) data;
        u32 pid = data_in ? TD_PID_IN : TD_PID_OUT;
        int toggle = 1;

        while (remaining > 0 && idx < 7) {   /* макс. 3 data TD */
            u32 chunk = remaining;
            if (chunk > max_packet) chunk = max_packet;

            fill_td(&ctrl_td[idx], pid, addr, 0, toggle, chunk,
                    (u32) ptr, 0);
            if (low_speed) ctrl_td[idx].status |= TD_ST_LS;
            prev->link = ((u32)&ctrl_td[idx]) | TD_LINK_VF;
            prev = &ctrl_td[idx];

            ptr       += chunk;
            remaining -= chunk;
            toggle ^= 1;      /* чередуем DATA0/DATA1 */
            idx++;
        }

        if (remaining > 0) {
            printf("[uhci] data too long for %d TDs\n", idx);
            return 0;
        }
    }

    /* ─── Status stage: 1 TD, maxlen=0x7FF, PID=противоположный. */
    u32 status_pid;
    if (data_len == 0)       status_pid = TD_PID_IN;
    else if (data_in)        status_pid = TD_PID_OUT;
    else                     status_pid = TD_PID_IN;

    fill_td(&ctrl_td[idx], status_pid, addr, 0, 1, 0x7FF,
            (u32) ctrl_data, 1);
    if (low_speed) ctrl_td[idx].status |= TD_ST_LS;
    prev->link = ((u32)&ctrl_td[idx]) | TD_LINK_VF;

    /* ─── Прикрепляем к idle_qh. ────────────────────── */
    idle_qh.qhel = (u32)&ctrl_td[0];

    printf("[uhci] xfer: %d TDs, last.status=0x%08x\n",
           idx + 1, ctrl_td[idx].status);

    int t;
    for (t = 0; t < 500; t++) {
        if (!(ctrl_td[idx].status & TD_ST_ACTIVE)) break;
        for (volatile int j = 0; j < 100000; j++) { }
    }

    idle_qh.qhel = TD_LINK_TERM;

    printf("[uhci] xfer: waited %d iters, TD0=0x%08x TDlast=0x%08x\n",
           t, ctrl_td[0].status, ctrl_td[idx].status);

    /* ─── Проверка всех TD. ─────────────────────────── */
    for (int k = 0; k <= idx; k++) {
        u32 st = ctrl_td[k].status;
        if (st & TD_ST_ACTIVE) {
            printf("[uhci] TD%d still ACTIVE\n", k);
            return 0;
        }
        if (st & TD_ST_ERRMASK) {
            printf("[uhci] TD%d ERR status=0x%08x", k, st);
            if (st & TD_ST_STALLED)  printf(" STALLED");
            if (st & TD_ST_DBERR)    printf(" DBERR");
            if (st & TD_ST_BABBLE)   printf(" BABBLE");
            if (st & TD_ST_NAK)      printf(" NAK");
            if (st & TD_ST_CRC)      printf(" CRC");
            if (st & TD_ST_BITSTUFF) printf(" BITSTUFF");
            printf("\n");
            return 0;
        }
    }
    printf("[uhci] xfer: all TDs OK\n");
    return 1;
}

/* ─── Первый enumeration ───────────────────────────── */

void uhci_test_enumerate(void) {
    printf("[usb] starting enumeration test...\n");

    if (!uhci_port_connected(1)) {
        printf("[usb] no device on port 1\n");
        return;
    }

    usb_setup_packet_t setup = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest      = USB_REQ_GET_DESCRIPTOR,
        .wValue        = (USB_DESC_DEVICE << 8) | 0,
        .wIndex        = 0,
        .wLength       = 8,
    };

    u8 buf[18];
    memset(buf, 0, sizeof(buf));

    printf("[usb] GET_DESCRIPTOR(device, first 8 bytes) @ addr 0...\n");
    if (!uhci_control_transfer(0, 0, 8, &setup, buf, 8, 1)) {
        printf("[usb] transfer failed\n");
        return;
    }

    printf("[usb] first 8 bytes: ");
    for (int i = 0; i < 8; i++) printf("%02x ", buf[i]);
    printf("\n");

    printf("[usb]   bLength=%u bDescriptorType=%u\n", buf[0], buf[1]);
    u8 max_pkt = buf[7];
    printf("[usb]   bMaxPacketSize0=%u\n", max_pkt);

    setup.bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
    setup.bRequest      = USB_REQ_SET_ADDRESS;
    setup.wValue        = 1;
    setup.wIndex        = 0;
    setup.wLength       = 0;

    printf("[usb] SET_ADDRESS(1)...\n");
    if (!uhci_control_transfer(0, 0, max_pkt ? max_pkt : 8, &setup,
        NULL, 0, 0)) {
        printf("[usb] SET_ADDRESS failed\n");
    return;
        }

        for (volatile int i = 0; i < 5000000; i++) { }

        setup.bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
        setup.bRequest      = USB_REQ_GET_DESCRIPTOR;
        setup.wValue        = (USB_DESC_DEVICE << 8) | 0;
        setup.wIndex        = 0;
        setup.wLength       = 18;

        printf("[usb] GET_DESCRIPTOR(device, full) @ addr 1...\n");
        if (!uhci_control_transfer(1, 0, max_pkt ? max_pkt : 8, &setup,
            buf, 18, 1)) {
            printf("[usb] full descriptor failed\n");
        return;
            }

            printf("[usb] device descriptor:\n");
            printf("  idVendor  = %04x\n", buf[8] | (buf[9] << 8));
            printf("  idProduct = %04x\n", buf[10] | (buf[11] << 8));
            printf("  bDeviceClass = %02x\n", buf[4]);
            printf("  bNumConfigurations = %u\n", buf[17]);

            printf("[usb] enumeration OK!\n");
}

/* ─── Bulk transfer ────────────────────────────────── */

static u8 toggle_bulk_in  = 0;
static u8 toggle_bulk_out = 0;

u8 uhci_get_toggle(u8 endpoint, int data_in) {
    (void) endpoint;
    return data_in ? toggle_bulk_in : toggle_bulk_out;
}

static void uhci_set_toggle(u8 endpoint, int data_in, u8 v) {
    (void) endpoint;
    if (data_in) toggle_bulk_in  = v;
    else         toggle_bulk_out = v;
}

int uhci_bulk_transfer(u8 addr, int low_speed, u8 endpoint, u8 max_packet,
                       void* data, u32 data_len, int data_in,
                       u8 start_toggle)
{
    if (!uhci_ok) return -1;
    if (max_packet == 0) max_packet = 64;
    if (data_len == 0) return 0;

    u32 num_packets = (data_len + max_packet - 1) / max_packet;
    if (num_packets > 16) {
        printf("[uhci] bulk too many packets: %u\n", num_packets);
        return -1;
    }

    static uhci_td_t bulk_td[16] __attribute__((aligned(16)));

    u8* ptr = (u8*) data;
    u32 remaining = data_len;
    u8 toggle = start_toggle;
    u32 pid = data_in ? TD_PID_IN : TD_PID_OUT;

    for (u32 i = 0; i < num_packets; i++) {
        u32 chunk = remaining > max_packet ? max_packet : remaining;
        fill_td(&bulk_td[i], pid, addr, endpoint & 0x0F,
                toggle, chunk, (u32) ptr, 0);
        if (low_speed) bulk_td[i].status |= TD_ST_LS;

        if (i + 1 < num_packets) {
            bulk_td[i].link = ((u32)&bulk_td[i + 1]) | TD_LINK_VF;
        } else {
            bulk_td[i].link = TD_LINK_TERM;
        }

        ptr       += chunk;
        remaining -= chunk;
        toggle    ^= 1;
    }

    idle_qh.qhel = (u32)&bulk_td[0];

    for (int t = 0; t < 500; t++) {
        if (!(bulk_td[num_packets - 1].status & TD_ST_ACTIVE)) break;
        for (volatile int j = 0; j < 100000; j++) { }
    }

    idle_qh.qhel = TD_LINK_TERM;

    /* Проверяем каждый TD. */
    u32 actual_total = 0;
    u32 remaining_data = data_len;

    for (u32 k = 0; k < num_packets; k++) {
        u32 chunk = remaining_data > max_packet ? max_packet : remaining_data;
        remaining_data -= chunk;

        u32 st = bulk_td[k].status;

        if (st & TD_ST_ACTIVE) {
            printf("[uhci] bulk TD%u still ACTIVE\n", k);
            return -1;
        }
        if (st & TD_ST_ERRMASK) {
            printf("[uhci] bulk TD%u ERR 0x%08x", k, st);
            if (st & TD_ST_STALLED) printf(" STALLED");
            if (st & TD_ST_CRC)     printf(" CRC");
            if (st & TD_ST_BABBLE)  printf(" BABBLE");
            printf("\n");
            return -1;
        }

        /* status & 0x7FF == chunk - 1 → полный трансфер.
         * Если меньше — короткий пакет, считаем сколько реально прошло. */
        u32 low = st & 0x7FF;
        u32 expected = (chunk - 1) & 0x7FF;

        if (low == expected) {
            actual_total += chunk;
        } else {
            /* Неопределённая семантика short packet для QEMU и железа.
             * Пока просто считаем, что пришло 0. */
            actual_total += 0;
            break;   /* дальнейшие пакеты не имеют смысла */
        }
    }

    uhci_set_toggle(endpoint, data_in, toggle);
    return (int) actual_total;
}
