// system/drivers/usb/usb_msc_bridge.c

#include <stdint.h>
#include <stddef.h>

#include "usbh_core.h"
#include "usbh_msc.h"
#include "usb_config.h"
#include "usb_log.h"

#include "printf.h"
#include "string.h"
#include "blockdev.h"
#include "kheap.h"

extern void process_sleep(uint32_t ms);
extern u32  process_create(const char *name, void (*fn)(void*), void *arg);

static int usb_msc_bdev_read(block_device_t *dev, u32 lba, u8 count, void *buf)
{
    struct usbh_msc *msc = (struct usbh_msc *)dev->private_data;
    if (!msc) return 0;
    int ret = usbh_msc_scsi_read10(msc, lba, (const uint8_t *)buf, count);
    return (ret == 0) ? 1 : 0;
}

static int usb_msc_bdev_write(block_device_t *dev, u32 lba, u8 count, const void *buf)
{
    struct usbh_msc *msc = (struct usbh_msc *)dev->private_data;
    if (!msc) return 0;
    int ret = usbh_msc_scsi_write10(msc, lba, (const uint8_t *)buf, count);
    return (ret == 0) ? 1 : 0;
}

static void usb_msc_thread(void *arg)
{
    (void)arg;

    // Ждём, пока hub-нить отенаммерит оба устройства.
    // Port 1 (HID) занимает ~2 сек, port 2 (MSC) — ещё ~1-2 сек.
    // 8 секунд — с запасом.
    process_sleep(8000);

    struct usbh_msc *msc = NULL;
    for (int i = 0; i < 30 && msc == NULL; i++) {
        msc = (struct usbh_msc *)usbh_find_class_instance("/dev/sda");
        if (msc == NULL) {
            process_sleep(200);
        }
    }
    if (msc == NULL) {
        printf("[usb-msc] no MSC device found (sda missing)\n");
        return;
    }

    printf("[usb-msc] found /dev/sda: blocknum=%u blocksize=%u\n",
           msc->blocknum, msc->blocksize);

    if (msc->blocknum == 0 || msc->blocksize == 0) {
        printf("[usb-msc] capacity unknown, calling scsi_init\n");
        usbh_msc_scsi_init(msc);
        process_sleep(200);
    }

    if (msc->blocknum == 0) {
        printf("[usb-msc] device reports 0 blocks, aborting\n");
        return;
    }

    block_device_t *bdev = (block_device_t *)kmalloc(sizeof(block_device_t));
    if (!bdev) {
        printf("[usb-msc] OOM for block_device\n");
        return;
    }
    memset(bdev, 0, sizeof(*bdev));
    strncpy(bdev->name, "usb0", BDEV_NAME_LEN - 1);
    bdev->sector_size   = msc->blocksize;
    bdev->total_sectors = msc->blocknum;
    bdev->read          = usb_msc_bdev_read;
    bdev->write         = usb_msc_bdev_write;
    bdev->private_data  = msc;

    if (!bdev_register(bdev)) {
        printf("[usb-msc] bdev_register failed\n");
        kfree(bdev);
        return;
    }

    printf("[usb-msc] registered block_device 'usb0' (%u sectors x %u bytes)\n",
           bdev->total_sectors, bdev->sector_size);
}

void usb_msc_bridge_init(void)
{
    process_create("usb_msc", usb_msc_thread, NULL);
}
