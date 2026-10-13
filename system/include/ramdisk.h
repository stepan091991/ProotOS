#ifndef PROOTOS_RAMDISK_H
#define PROOTOS_RAMDISK_H

#include "blockdev.h"

/* Создаёт RAM-диск поверх физической памяти [addr, addr+size).
 * Регистрирует его как блочное устройство. */
block_device_t* ramdisk_create(const char* name, u32 addr, u32 size);

#endif
