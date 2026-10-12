#include "blockdev.h"
#include "printf.h"
#include "string.h"

static block_device_t* devices[BDEV_MAX_DEVICES];
static int device_count = 0;

int bdev_register(block_device_t* dev) {
    if (device_count >= BDEV_MAX_DEVICES) return 0;
    if (!dev || !dev->read) return 0;
    devices[device_count++] = dev;
    return 1;
}

block_device_t* bdev_get(const char* name) {
    for (int i = 0; i < device_count; i++) {
        if (strcmp(devices[i]->name, name) == 0) return devices[i];
    }
    return NULL;
}

block_device_t* bdev_get_by_index(int idx) {
    if (idx < 0 || idx >= device_count) return NULL;
    return devices[idx];
}

int bdev_count(void) { return device_count; }

int bdev_read(block_device_t* dev, u32 lba, u8 count, void* buf) {
    if (!dev || !dev->read) return 0;
    return dev->read(dev, lba, count, buf);
}

int bdev_write(block_device_t* dev, u32 lba, u8 count, const void* buf) {
    if (!dev || !dev->write) return 0;
    return dev->write(dev, lba, count, buf);
}

void bdev_dump(void) {
    printf("[bdev] %d device(s):\n", device_count);
    for (int i = 0; i < device_count; i++) {
        block_device_t* d = devices[i];
        printf("  [%d] %-12s  %u sectors  (%u KB)%s\n",
               i, d->name, d->total_sectors,
               (d->total_sectors * d->sector_size) / 1024,
               d->write ? "" : "  (read-only)");
    }
}
