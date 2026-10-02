ENTRY(_start)

MEMORY
{
    /* System/Text Partition: Core 0 Boot code, constants, .bss, arbiter logic */
    RAM (rwx)   : ORIGIN = 0x80000000, LENGTH = 16M
    
    /* Spatial Memory Partitioning: Dedicated hardware zones */
    ZONE1 (rw)  : ORIGIN = 0x81000000, LENGTH = 4K
    ZONE2 (rw)  : ORIGIN = 0x82000000, LENGTH = 4K
    ZONE3 (rw)  : ORIGIN = 0x83000000, LENGTH = 4K
}

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    /* Reset and Interrupt Vector Table at RAM Base */
    .text : {
        KEEP(*(.vector_table))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > RAM : text
    _text_start = ADDR(.text);
    _text_end = .;

    .data : {
        *(.data .data.*)
        . = ALIGN(4);
    } > RAM : data

    .bss (NOLOAD) : {
        . = ALIGN(4);
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM : data

    /* 32KB total stack: 8KB per core (6KB SVC, 2KB banked exception modes) */
    .stack (NOLOAD) : {
        . = ALIGN(8);
        _stack_bottom = .;
        . += 0x8000; /* 32KB */
        _stack_top = .;
    } > RAM : data

    /* Zone Boundary Symbols */
    _zone1_in  = ORIGIN(ZONE1);
    _zone1_out = ORIGIN(ZONE1) + 4;
    _zone2_in  = ORIGIN(ZONE2);
    _zone2_out = ORIGIN(ZONE2) + 4;
    _zone3_in  = ORIGIN(ZONE3);
    _zone3_out = ORIGIN(ZONE3) + 4;

    /* Spatial Hardware Partitioning: Mailbox channels in dedicated RAM zones */
    .mailbox_core1 (ORIGIN(ZONE1) + 0x100) (NOLOAD) : {
        . = ALIGN(4);
        *(.mailbox.core1)
    } > ZONE1 : NONE

    .mailbox_core2 (ORIGIN(ZONE2) + 0x100) (NOLOAD) : {
        . = ALIGN(4);
        *(.mailbox.core2)
    } > ZONE2 : NONE

    .mailbox_core3 (ORIGIN(ZONE3) + 0x100) (NOLOAD) : {
        . = ALIGN(4);
        *(.mailbox.core3)
    } > ZONE3 : NONE

    /* Discard unused sections */
    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.gnu.attributes)
        *(.comment)
    }
}
