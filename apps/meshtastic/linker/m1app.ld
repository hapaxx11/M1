/*
 * m1app.ld -- Linker script for M1 external apps (.m1app)
 *
 * Apps are compiled as relocatable ELF objects (ET_REL). The M1 firmware
 * loads them into RAM, applies relocations, and resolves symbols at runtime.
 *
 * This script defines the section layout. There is no MEMORY region because
 * the final addresses are determined by the loader at runtime.
 */

ENTRY(app_main)

SECTIONS
{
    /* Code */
    .text : {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.glue_7)
        *(.glue_7t)
        . = ALIGN(4);
    }

    /* Read-only data */
    .rodata : {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
    }

    /* App manifest — read by the M1 loader before execution */
    .m1meta : {
        . = ALIGN(4);
        KEEP(*(.m1meta))
        . = ALIGN(4);
    }

    /* Initialized data */
    .data : {
        . = ALIGN(4);
        *(.data)
        *(.data.*)
        . = ALIGN(4);
    }

    /* Zero-initialized data */
    .bss : {
        . = ALIGN(4);
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
    }

    /* ARM exception tables (not used, but keep the linker happy) */
    .ARM.exidx : {
        *(.ARM.exidx*)
    }

    .ARM.extab : {
        *(.ARM.extab*)
    }

    /* Discard sections we don't need */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.debug*)
        *(.ARM.attributes)
    }
}
