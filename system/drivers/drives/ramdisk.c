#include "ramdisk.h"
#include "printf.h"
#include "string.h"

typedef struct {
    u8*  base;
    u32  size;
} ramdisk_data_t;

static int rd_read(block_device_t* dev, u32 lba, u8 count, void* buf) {
    ramdisk_data_t* rd = (ramdisk_data_t*) dev->private_data;
    u32 offset = lba * dev->sector_size;
    u32 len    = (u32) count * dev->sector_size;
    if (offset + len > rd->size) return 0;
    memcpy(buf, rd->base + offset, len);
    return 1;
}

static int rd_write(block_device_t* dev, u32 lba, u8 count, const void* buf) {
    ramdisk_data_t* rd = (ramdisk_data_t*) dev->private_data;
    u32 offset = lba * dev->sector_size;
    u32 len    = (u32) count * dev->sector_size;
    if (offset + len > rd->size) return 0;
    memcpy(rd->base + offset, buf, len);
    return 1;
}

/* У нас ровно один initrd — статическая структура. */
static ramdisk_data_t rd_data;
static block_device_t rd_dev;

block_device_t* ramdisk_create(const char* name, u32 addr, u32 size) {
    rd_data.base = (u8*) addr;
    rd_data.size = size;

    strncpy(rd_dev.name, name, BDEV_NAME_LEN - 1);
    rd_dev.name[BDEV_NAME_LEN - 1] = '\0';
    rd_dev.sector_size   = 512;
    rd_dev.total_sectors = size / 512;
    rd_dev.read          = rd_read;
    rd_dev.write         = rd_write;
    rd_dev.private_data  = &rd_data;

    if (!bdev_register(&rd_dev)) return NULL;

    printf("[ramdisk] '%s' at %p, %u KB, %u sectors (RW)\n",
           name, (void*) addr, size / 1024, rd_dev.total_sectors);
    return &rd_dev;
}
