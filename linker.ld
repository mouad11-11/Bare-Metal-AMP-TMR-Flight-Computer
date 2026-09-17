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

SECTIONS
{
    /* Reset and Interrupt Vector Table at RAM Base */
    .text : {
        KEEP(*(.vector_table))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > RAM

    .data : {
        *(.data .data.*)
        . = ALIGN(4);
    } > RAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM

    /* 16KB total stack allocated: 4KB strictly isolated per core */
    .stack (NOLOAD) : {
        . = ALIGN(8);
        _stack_bottom = .;
        . += 0x4000; /* 16KB */
        _stack_top = .;
    } > RAM

    /* Zone Boundary Symbols */
    _zone1_in  = ORIGIN(ZONE1);
    _zone1_out = ORIGIN(ZONE1) + 4;
    _zone2_in  = ORIGIN(ZONE2);
    _zone2_out = ORIGIN(ZONE2) + 4;
    _zone3_in  = ORIGIN(ZONE3);
    _zone3_out = ORIGIN(ZONE3) + 4;

    /* Discard unused sections */
    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.gnu.attributes)
        *(.comment)
    }
}
