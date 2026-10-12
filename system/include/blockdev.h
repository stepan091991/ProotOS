#ifndef PROOTOS_BLOCKDEV_H
#define PROOTOS_BLOCKDEV_H

#include "types.h"

#define BDEV_MAX_DEVICES 8
#define BDEV_NAME_LEN    16

struct block_device;

typedef int (*bdev_read_fn)(struct block_device* dev, u32 lba, u8 count, void* buf);
typedef int (*bdev_write_fn)(struct block_device* dev, u32 lba, u8 count, const void* buf);

typedef struct block_device {
    char           name[BDEV_NAME_LEN];
    u32            sector_size;
    u32            total_sectors;
    bdev_read_fn   read;
    bdev_write_fn  write;
    void*          private_data;
} block_device_t;

int              bdev_register(block_device_t* dev);
block_device_t*  bdev_get(const char* name);
block_device_t*  bdev_get_by_index(int idx);
int              bdev_count(void);
void             bdev_dump(void);

int  bdev_read(block_device_t* dev, u32 lba, u8 count, void* buf);
int  bdev_write(block_device_t* dev, u32 lba, u8 count, const void* buf);

#endif
