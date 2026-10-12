#ifndef PROOTOS_PIC_H
#define PROOTOS_PIC_H

#include "types.h"

/* Векторы, на которые после ремапа приходят IRQ */
#define PIC_IRQ_BASE    32
#define PIC_IRQ_COUNT   16

/* Границы master/slave */
#define PIC_MASTER_VECTOR 32
#define PIC_SLAVE_VECTOR  40

/* Инициализация и ремап PIC */
void pic_init(void);

/* Послать End-Of-Interrupt для указанного IRQ */
void pic_send_eoi(u8 irq);

/* Замаскировать/размаскировать IRQ (irq = 0..15) */
void pic_set_mask(u8 irq);
void pic_clear_mask(u8 irq);
void pic_mask_all(void);

#endif
