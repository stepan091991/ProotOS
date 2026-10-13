#ifndef PROOTOS_ATA_H
#define PROOTOS_ATA_H

#include "types.h"

#define ATA_SECTOR_SIZE 512

int  ata_init(void);
int  ata_read_sectors(u8 drive, u32 lba, u8 count, void* buffer);
int  ata_write_sectors(u8 drive, u32 lba, u8 count, const void* buffer);
void ata_dump_info(void);
void ata_test(void);

/* Регистрирует обнаруженные ATA-диски как block_device. */
void ata_register_block_devices(void);

#endif
