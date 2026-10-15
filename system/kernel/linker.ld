ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    . = 1M;

    .text ALIGN(4K) : {
        *(.multiboot)
        *(.text)
    }

    /* CherryUSB: авто-регистрация классов (HID, MSC) */
    .usbh_class_info : ALIGN(4) {
        __usbh_class_info_start__ = .;
        KEEP(*(SORT_BY_NAME(".usbh_class_info*")))
        KEEP(*(.usbh_class_info))
        __usbh_class_info_end__ = .;
    }

    .rodata ALIGN(4K) : {
        *(.rodata)
    }

    .data ALIGN(4K) : {
        *(.data)
    }

    .bss ALIGN(4K) : {
        *(.bss)
        *(COMMON)
    }

    . = ALIGN(4K);
    kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
