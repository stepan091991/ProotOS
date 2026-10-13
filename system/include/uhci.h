#ifndef PROOTOS_UHCI_H
#define PROOTOS_UHCI_H

#include "types.h"
#include "usb.h"

int uhci_init(void);
int uhci_port_connected(int port);
int uhci_port_reset(int port);

int uhci_control_transfer(u8 addr, int low_speed, u8 max_packet,
                          const usb_setup_packet_t* setup,
                          void* data, u32 data_len, int data_in);

/* Bulk transfer. endpoint — адрес endpoint (включая бит направления).
 * Возвращает число переданных байт или -1 при ошибке. */
int uhci_bulk_transfer(u8 addr, int low_speed, u8 endpoint, u8 max_packet,
                       void* data, u32 data_len, int data_in,
                       u8 start_toggle);

void uhci_dump(void);
void uhci_test_enumerate(void);

/* Получить и обновить toggle для endpoint.
 * Это внутреннее состояние UHCI, но нужно BOT'у для корректного
 * протокола (CBW/CSW начинают с DATA0). */
u8  uhci_get_toggle(u8 endpoint, int data_in);

#endif
