#include "usb_msd.h"
#include "usb.h"
#include "uhci.h"
#include "printf.h"
#include "string.h"
#include "blockdev.h"

static usb_msd_info_t info;

#define USB_DEV_ADDR    1
#define CONFIG_BUF_SIZE 256

static u8 config_buf[CONFIG_BUF_SIZE];

/* ─── Control helpers ──────────────────────────────── */

static int ctrl_in(u8 bmRequestType, u8 bRequest,
                   u16 wValue, u16 wIndex, u16 wLength, void* buf)
{
    usb_setup_packet_t s = {
        .bmRequestType = bmRequestType,
        .bRequest      = bRequest,
        .wValue        = wValue,
        .wIndex        = wIndex,
        .wLength       = wLength,
    };
    return uhci_control_transfer(USB_DEV_ADDR, 0, 64, &s, buf, wLength, 1);
}

static int ctrl_out(u8 bmRequestType, u8 bRequest,
                    u16 wValue, u16 wIndex, u16 wLength, void* buf)
{
    usb_setup_packet_t s = {
        .bmRequestType = bmRequestType,
        .bRequest      = bRequest,
        .wValue        = wValue,
        .wIndex        = wIndex,
        .wLength       = wLength,
    };
    return uhci_control_transfer(USB_DEV_ADDR, 0, 64, &s, buf, wLength, 0);
}

static int get_config_descriptor(u16 wLength, void* buf) {
    return ctrl_in(USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                   USB_REQ_GET_DESCRIPTOR,
                   (USB_DESC_CONFIGURATION << 8) | 0,
                   0, wLength, buf);
}

/* ─── Разбор config descriptor ─────────────────────── */

static int parse_config(const u8* buf, u32 total_len) {
    u32 off = 0;
    int found_msd = 0, found_in = 0, found_out = 0;

    while (off < total_len) {
        if (off + 2 > total_len) break;
        u8 bLength = buf[off];
        u8 bType   = buf[off + 1];
        if (bLength == 0 || off + bLength > total_len) break;

        if (bType == USB_DESC_INTERFACE) {
            usb_interface_descriptor_t* id =
            (usb_interface_descriptor_t*)(buf + off);
            printf("[usb] interface %u: class=%02x sub=%02x proto=%02x eps=%u\n",
                   id->bInterfaceNumber, id->bInterfaceClass,
                   id->bInterfaceSubClass, id->bInterfaceProtocol,
                   id->bNumEndpoints);
            if (id->bInterfaceClass    == USB_CLASS_MASS_STORAGE &&
                id->bInterfaceSubClass == USB_MS_SCSI &&
                id->bInterfaceProtocol == USB_MS_PROTO_BOT) {
                found_msd = 1;
                }
        } else if (bType == USB_DESC_ENDPOINT) {
            usb_endpoint_descriptor_t* ed =
            (usb_endpoint_descriptor_t*)(buf + off);
            u8 ep = ed->bEndpointAddress;
            u8 attr = ed->bmAttributes & 0x03;
            printf("[usb]   ep 0x%02x attr=%u max_pkt=%u\n",
                   ep, attr, ed->wMaxPacketSize);
            if (attr == USB_EP_TYPE_BULK) {
                if (ep & 0x80) { info.ep_in  = ep; found_in  = 1; }
                else           { info.ep_out = ep; found_out = 1; }
                info.max_packet = ed->wMaxPacketSize;
            }
        }
        off += bLength;
    }
    return (found_msd && found_in && found_out) ? 1 : 0;
}

/* ─── BOT: CBW / CSW ───────────────────────────────── */

typedef struct {
    u32 dCBWSignature;         /* 0x43425355 = 'USBC' */
    u32 dCBWTag;
    u32 dCBWDataTransferLength;
    u8  bmCBWFlags;            /* bit 7: 1=IN, 0=OUT */
    u8  bCBWLUN;               /* 0 */
    u8  bCBWCBLength;          /* 1..16 */
    u8  CBWCB[16];
} __attribute__((packed)) usb_cbw_t;

typedef struct {
    u32 dCSWSignature;         /* 0x53425355 = 'USBS' */
    u32 dCSWTag;
    u32 dCSWDataResidue;
    u8  bCSWStatus;            /* 0=OK, 1=fail, 2=phase err */
} __attribute__((packed)) usb_csw_t;

static u32 bot_tag = 1;

/*
 * Отправить SCSI-команду через BOT.
 *   cmd       — массив SCSI-байтов
 *   cmd_len   — длина команды (6, 10, 12)
 *   data      — буфер данных (может быть NULL)
 *   data_len  — сколько байт ожидаем/передаём
 *   data_in   — 1 для IN (устройство → хост), 0 для OUT
 *   sense     — 18-байтовый массив для sense info (пока игнорируется)
 * Возвращает 1 при успехе.
 */
static int bot_scsi_command(const u8* cmd, u8 cmd_len,
                            void* data, u32 data_len, int data_in)
{
    usb_cbw_t cbw;
    usb_csw_t csw;

    memset(&cbw, 0, sizeof(cbw));
    cbw.dCBWSignature          = 0x43425355;
    cbw.dCBWTag                = bot_tag++;
    cbw.dCBWDataTransferLength = data_len;
    cbw.bmCBWFlags             = data_in ? 0x80 : 0x00;
    cbw.bCBWLUN                = 0;
    cbw.bCBWCBLength           = cmd_len;
    memcpy(cbw.CBWCB, cmd, cmd_len);

    /* 1. CBW — bulk OUT, 31 байт. */
    int r = uhci_bulk_transfer(USB_DEV_ADDR, 0, info.ep_out,
                               info.max_packet, &cbw, 31, 0, 0);
    if (r != 31) {
        printf("[msd] CBW send failed (%d)\n", r);
        return 0;
    }

    /* 2. Data stage — если data_len > 0. */
    if (data_len > 0 && data != NULL) {
        r = uhci_bulk_transfer(USB_DEV_ADDR, 0,
                               data_in ? info.ep_in : info.ep_out,
                               info.max_packet,
                               data, data_len, data_in, 1);
        if (r < 0) {
            printf("[msd] data transfer failed (%d)\n", r);
            return 0;
        }
        if ((u32) r != data_len) {
            printf("[msd] data short: %d / %u\n", r, data_len);
            /* Не критично для IN — устройство может отдать меньше,
             * если мы запросили больше данных, чем есть. */
            if (!data_in) return 0;
        }
    }

    /* 3. CSW — bulk IN, 13 байт. */
    r = uhci_bulk_transfer(USB_DEV_ADDR, 0, info.ep_in,
                           info.max_packet, &csw, 13, 1, 0);
    if (r != 13) {
        printf("[msd] CSW read failed (%d)\n", r);
        return 0;
    }

    if (csw.dCSWSignature != 0x53425355) {
        printf("[msd] bad CSW signature: 0x%08x\n", csw.dCSWSignature);
        return 0;
    }
    if (csw.dCSWTag != cbw.dCBWTag) {
        printf("[msd] CSW tag mismatch\n");
        return 0;
    }
    if (csw.bCSWStatus != 0) {
        printf("[msd] CSW status = %u (residue=%u)\n",
               csw.bCSWStatus, csw.dCSWDataResidue);
        return 0;
    }
    return 1;
}

/* ─── SCSI команды ─────────────────────────────────── */

static int scsi_inquiry(void) {
    u8 cmd[6] = { 0x12, 0, 0, 0, 36, 0 };
    u8 data[36];
    memset(data, 0, sizeof(data));

    if (!bot_scsi_command(cmd, 6, data, 36, 1)) return 0;

    /* data[8..15] — vendor (8 ASCII), data[16..31] — product (16 ASCII) */
    char vendor[9], product[17];
    memcpy(vendor,  &data[8],  8); vendor[8]   = '\0';
    memcpy(product, &data[16], 16); product[16] = '\0';

    printf("[msd] INQUIRY: vendor='%s' product='%s'\n", vendor, product);
    return 1;
}

static int scsi_read_capacity(void) {
    u8 cmd[10] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    u8 data[8];

    if (!bot_scsi_command(cmd, 10, data, 8, 1)) return 0;

    /* Big-endian */
    u32 last_lba =
    ((u32)data[0] << 24) | ((u32)data[1] << 16) |
    ((u32)data[2] <<  8) | ((u32)data[3]);
    u32 block_size =
    ((u32)data[4] << 24) | ((u32)data[5] << 16) |
    ((u32)data[6] <<  8) | ((u32)data[7]);

    info.num_blocks = last_lba + 1;
    info.block_size = block_size;

    printf("[msd] READ CAPACITY: %u blocks × %u bytes = %u MB\n",
           info.num_blocks, info.block_size,
           (info.num_blocks * info.block_size) / (1024 * 1024));
    return 1;
}

/* Публичные read/write. */
int usb_msd_read_sectors(u32 lba, u32 count, void* buf) {
    if (!info.configured) return 0;
    if (count == 0) return 1;
    if (count > 0xFFFF) count = 0xFFFF;

    u8 cmd[10] = { 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    /* LBA big-endian: bytes 2..5 */
    cmd[2] = (lba >> 24) & 0xFF;
    cmd[3] = (lba >> 16) & 0xFF;
    cmd[4] = (lba >>  8) & 0xFF;
    cmd[5] =  lba        & 0xFF;
    /* Transfer length big-endian: bytes 7..8 */
    cmd[7] = (count >> 8) & 0xFF;
    cmd[8] =  count       & 0xFF;

    u32 bytes = count * 512;
    return bot_scsi_command(cmd, 10, buf, bytes, 1);
}

int usb_msd_write_sectors(u32 lba, u32 count, const void* buf) {
    if (!info.configured) return 0;
    if (count == 0) return 1;
    if (count > 0xFFFF) count = 0xFFFF;

    u8 cmd[10] = { 0x2A, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    cmd[2] = (lba >> 24) & 0xFF;
    cmd[3] = (lba >> 16) & 0xFF;
    cmd[4] = (lba >>  8) & 0xFF;
    cmd[5] =  lba        & 0xFF;
    cmd[7] = (count >> 8) & 0xFF;
    cmd[8] =  count       & 0xFF;

    u32 bytes = count * 512;
    return bot_scsi_command(cmd, 10, (void*) buf, bytes, 0);
}

/* ─── block_device wrapper ─────────────────────────── */

static int usb_bdev_read(block_device_t* dev, u32 lba, u8 count, void* buf) {
    (void) dev;
    return usb_msd_read_sectors(lba, count, buf);
}

static int usb_bdev_write(block_device_t* dev, u32 lba, u8 count,
                          const void* buf) {
    (void) dev;
    return usb_msd_write_sectors(lba, count, buf);
                          }

                          static block_device_t usb_bdev;

                          static void register_block_device(void) {
                              strcpy(usb_bdev.name, "usb0");
                              usb_bdev.sector_size   = 512;
                              usb_bdev.total_sectors = info.num_blocks;
                              usb_bdev.read          = usb_bdev_read;
                              usb_bdev.write         = usb_bdev_write;
                              usb_bdev.private_data  = NULL;
                              bdev_register(&usb_bdev);
                          }

                          /* ─── Публичный API ────────────────────────────────── */

                          int usb_msd_init(void) {
                              memset(&info, 0, sizeof(info));

                              /* 1. GET_DESCRIPTOR(device, 8). */
                              {
                                  usb_setup_packet_t s = {
                                      .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                                      .bRequest      = USB_REQ_GET_DESCRIPTOR,
                                      .wValue        = (USB_DESC_DEVICE << 8) | 0,
                                      .wIndex        = 0,
                                      .wLength       = 8,
                                  };
                                  u8 buf[8];
                                  if (!uhci_control_transfer(0, 0, 8, &s, buf, 8, 1)) {
                                      printf("[msd] cannot get device descriptor\n");
                                      return 0;
                                  }
                                  printf("[msd] bMaxPacketSize0=%u\n", buf[7]);

                                  /* 2. SET_ADDRESS(1). */
                                  s.bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
                                  s.bRequest      = USB_REQ_SET_ADDRESS;
                                  s.wValue        = 1;
                                  s.wIndex        = 0;
                                  s.wLength       = 0;

                                  if (!uhci_control_transfer(0, 0, 8, &s, NULL, 0, 0)) {
                                      printf("[msd] SET_ADDRESS(1) failed\n");
                                      return 0;
                                  }
                                  for (volatile int i = 0; i < 5000000; i++) { }
                                  printf("[msd] device now at address 1\n");
                              }

                              /* 3. Config descriptor. */
                              if (!get_config_descriptor(9, config_buf)) {
                                  printf("[msd] cannot get first 9 bytes of config\n");
                                  return 0;
                              }
                              usb_config_descriptor_t* cd = (usb_config_descriptor_t*) config_buf;
                              u16 total = cd->wTotalLength;
                              printf("[msd] total config length = %u\n", total);
                              if (total > CONFIG_BUF_SIZE) return 0;

                              if (!get_config_descriptor(total, config_buf)) {
                                  printf("[msd] cannot get full config\n");
                                  return 0;
                              }

                              if (!parse_config(config_buf, total)) {
                                  printf("[msd] not Mass Storage BOT\n");
                                  return 0;
                              }
                              printf("[msd] MSD BOT: in=0x%02x out=0x%02x max_pkt=%u\n",
                                     info.ep_in, info.ep_out, info.max_packet);

                              /* 4. SET_CONFIGURATION(1). */
                              if (!ctrl_out(USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                                  USB_REQ_SET_CONFIGURATION,
                                  cd->bConfigurationValue, 0, 0, NULL)) {
                                  printf("[msd] SET_CONFIGURATION failed\n");
                              return 0;
                                  }
                                  info.configured = 1;
                                  printf("[msd] configured OK\n");

                                  /* 5. SCSI: INQUIRY + READ CAPACITY. */
                                  if (!scsi_inquiry())       { printf("[msd] INQUIRY failed\n"); return 0; }
                                  if (!scsi_read_capacity()) { printf("[msd] READ CAPACITY failed\n"); return 0; }

                                  /* 6. Регистрируем block_device. */
                                  register_block_device();
                                  printf("[msd] registered as block device 'usb0'\n");

                                  return 1;
                          }

                          usb_msd_info_t* usb_msd_get_info(void) { return &info; }

                          void usb_msd_dump(void) {
                              if (!info.configured) {
                                  printf("[msd] not configured\n");
                                  return;
                              }
                              printf("[msd] ep_in=0x%02x ep_out=0x%02x max_pkt=%u\n",
                                     info.ep_in, info.ep_out, info.max_packet);
                              printf("[msd] %u blocks × %u = %u MB\n",
                                     info.num_blocks, info.block_size,
                                     (info.num_blocks * info.block_size) / (1024 * 1024));
                          }
