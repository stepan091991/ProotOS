#ifndef PROOTOS_USB_MSD_H
#define PROOTOS_USB_MSD_H

#include "types.h"

/* Состояние после успешной инициализации флешки. */
typedef struct {
    u8  configured;
    u8  ep_in;       /* bulk-in endpoint (например 0x81) */
    u8  ep_out;      /* bulk-out endpoint (например 0x02) */
    u16 max_packet;  /* max packet size endpoints */
    u32 num_blocks;  /* всего секторов */
    u32 block_size;  /* размер сектора (обычно 512) */
} usb_msd_info_t;

/* Запустить полный USB MSD enumeration.
 * Возвращает 1 при успехе. */
int usb_msd_init(void);

/* Прочитать секторы с флешки. 1 = успех. */
int usb_msd_read_sectors(u32 lba, u32 count, void* buf);

/* Записать секторы. */
int usb_msd_write_sectors(u32 lba, u32 count, const void* buf);

/* Состояние. */
usb_msd_info_t* usb_msd_get_info(void);

/* Утилита: печатает информацию о флешке. */
void usb_msd_dump(void);

#endif
