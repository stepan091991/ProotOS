#include "isr.h"
#include "printf.h"
#include "panic.h"
#include "string.h"
#include "vga.h"

/* Таблица пользовательских обработчиков — NULL означает «дефолт» */
static isr_t handlers[256] = { NULL };

/* Имена исключений для красивого вывода */
static const char* exception_names[32] = {
    "Divide Error",                   /* 0 */
    "Debug",                          /* 1 */
    "Non-Maskable Interrupt",         /* 2 */
    "Breakpoint",                     /* 3 */
    "Overflow",                       /* 4 */
    "Bound Range Exceeded",           /* 5 */
    "Invalid Opcode",                 /* 6 */
    "Device Not Available",           /* 7 */
    "Double Fault",                   /* 8 */
    "Coprocessor Segment Overrun",    /* 9 */
    "Invalid TSS",                    /* 10 */
    "Segment Not Present",            /* 11 */
    "Stack-Segment Fault",            /* 12 */
    "General Protection Fault",       /* 13 */
    "Page Fault",                     /* 14 */
    "Reserved",                       /* 15 */
    "x87 FPU Error",                  /* 16 */
    "Alignment Check",                /* 17 */
    "Machine Check",                  /* 18 */
    "SIMD FP Exception",              /* 19 */
    "Virtualization Exception",       /* 20 */
    "Control Protection",             /* 21 */
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

void isr_register_handler(u8 num, isr_t handler) {
    handlers[num] = handler;
}

/* Печатает «дамп регистров» — очень помогает при отладке */
static void dump_registers(registers_t* r) {
    printf("\n  int_no=%u  err_code=%p\n", r->int_no, (void*) r->err_code);
    printf("  eip=%p  cs=%p  eflags=%p\n",
           (void*) r->eip, (void*) r->cs, (void*) r->eflags);
    printf("  eax=%p  ebx=%p  ecx=%p  edx=%p\n",
           (void*) r->eax, (void*) r->ebx, (void*) r->ecx, (void*) r->edx);
    printf("  esi=%p  edi=%p  ebp=%p  esp=%p\n",
           (void*) r->esi, (void*) r->edi, (void*) r->ebp, (void*) r->esp_dummy);
    printf("  ds=%p  es=%p  fs=%p  gs=%p\n",
           (void*) r->ds, (void*) r->es, (void*) r->fs, (void*) r->gs);
}

void isr_handler(registers_t* regs) {
    /* Пользовательский обработчик имеет приоритет */
    if (handlers[regs->int_no] != NULL) {
        handlers[regs->int_no](regs);
        return;
    }

    if (regs->int_no == 0x80) {
        extern void syscall_handler(registers_t*);
        syscall_handler(regs);
        return;
    }

    /* Дефолтное поведение — фатальная ошибка для исключений 0..31 */
    if (regs->int_no < 32) {
        vga_set_color(VGA_WHITE, VGA_RED);
        printf("\n\n *** EXCEPTION %u: %s *** \n",
               regs->int_no, exception_names[regs->int_no]);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

        dump_registers(regs);

        /* Специфические сообщения для некоторых исключений */
        if (regs->int_no == 14) {
            /* Page fault: err_code расшифровывается */
            u32 ec = regs->err_code;
            printf("\n  Page fault details:\n");
            printf("    %s (P=%u)\n",
                   (ec & 1) ? "protection violation" : "page not present",
                   ec & 1);
            printf("    %s (W/R=%u)\n",
                   (ec & 2) ? "write" : "read", (ec >> 1) & 1);
            printf("    %s mode (U/S=%u)\n",
                   (ec & 4) ? "user" : "kernel", (ec >> 2) & 1);
            u32 cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            printf("    faulting address (CR2) = %p\n", (void*) cr2);
        }

        panic("unhandled CPU exception");
    }

    /* Для векторов >= 32 (IRQ и т.п.) — пока просто игнорируем.
     * Сюда мы попадём только после настройки PIC и разрешения прерываний. */
    printf("[isr] unhandled vector %u (ignored)\n", regs->int_no);
}
