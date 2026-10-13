ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    . = 0xD0000000;

    .text   : { *(.text) }
    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) *(COMMON) }

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
