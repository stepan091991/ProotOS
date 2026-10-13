#ifndef PROOTOS_ISR_H
#define PROOTOS_ISR_H

#include "types.h"

/*
 * Структура, которую видит C-обработчик на стеке.
 * Порядок полей СТРОГО соответствует порядку push'ей в ассемблерном стабе.
 */
typedef struct {
    /* сегменты, пушатся в обратном порядке: gs последним, значит первый в памяти */
    u32 gs, fs, es, ds;
    /* регистры, сохранённые pusha (EDI первым, EAX последним) */
    u32 edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    /* номер прерывания и error code — пушатся стабом */
    u32 int_no, err_code;
    /* то, что процессор положил сам при входе в обработчик */
    u32 eip, cs, eflags, user_esp, ss;
} registers_t;

typedef void (*isr_t)(registers_t*);

/* Регистрирует пользовательский обработчик на конкретный вектор */
void isr_register_handler(u8 num, isr_t handler);

/* Главный C-обработчик, вызывается из ассемблерного стаба */
void isr_handler(registers_t* regs);

#endif
