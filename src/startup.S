.syntax unified
.arch armv7-a
.cpu cortex-a15
.arm

.global _start
.global _stack_top
.global _stack_bottom

.section .vector_table, "ax"
.align 5
_vector_table:
    b _start            /* Reset */
    b _undef_handler    /* Undefined Instruction */
    b _svc_handler      /* Software Interrupt / SVC */
    b _pabt_handler     /* Prefetch Abort */
    b _dabt_handler     /* Data Abort */
    b .                 /* Reserved */
    b _irq_handler      /* IRQ */
    b _fiq_handler      /* FIQ */

.section .text
.align 2

_start:
    /* Set Vector Base Address Register (VBAR) */
    ldr r0, =_vector_table
    mcr p15, 0, r0, c12, c0, 0
    isb

    /* Enable Coprocessor Access (CP10, CP11) for VFP / NEON */
    mrc p15, 0, r0, c1, c0, 2
    orr r0, r0, #(0xF << 20)
    mcr p15, 0, r0, c1, c0, 2
    isb

    /* Enable VFP and NEON execution in FPEXC (bit 30 = EN) */
    mov r0, #(1 << 30)
    vmsr fpexc, r0

    /* Read Multiprocessor Affinity Register (MPIDR) */
    mrc p15, 0, r0, c0, c0, 5

    /*
     * Extract Affinity Levels:
     * Level 0 (Bits [7:0]): CPU ID
     * Level 1 (Bits [15:8]): Cluster ID
     * Linear Core ID = (Cluster * 4) + CPU
     */
    lsr r1, r0, #8
    and r1, r1, #0xFF           /* r1 = Cluster ID */
    and r0, r0, #0xFF           /* r0 = CPU ID */
    add r0, r0, r1, lsl #2      /* r0 = (Cluster << 2) + CPU */

    /*
     * Dynamic Stack Pointer Calculation:
     * sp = _stack_top - (core_id * 4096)
     * Each core is assigned an isolated 4KB stack partition.
     */
    ldr r1, =_stack_top
    mov r2, #4096
    mul r3, r0, r2
    sub sp, r1, r3

    /* Core 0 branches to primary initialization; Cores 1-3 park in WFE loop */
    cmp r0, #0
    bne secondary_boot

primary_boot:
    /* Zero the .bss section */
    ldr r1, =_bss_start
    ldr r2, =_bss_end
    mov r3, #0
1:
    cmp r1, r2
    bge 2f
    str r3, [r1], #4
    b 1b
2:
    /* Call C main() for Core 0 (Master Arbiter) */
    bl main

    /* If main returns, enter idle sleep */
halt_loop:
    wfe
    b halt_loop

secondary_boot:
    /* Cores 1, 2, and 3: Redundant Compute Nodes */
    /* Notify Core 0 that this secondary node is online */
    bl secondary_core_boot_notify

secondary_wait_loop:
    /*
     * Low-power spinloop using WFE (Wait For Event).
     * Secondary cores poll shared memory mailbox (secondary_spin_addr).
     */
    wfe
    ldr r1, =secondary_spin_addr
    ldr r2, [r1]
    cmp r2, #0
    beq secondary_wait_loop

    /* Branch to task entry point */
    blx r2

    /*
     * Handshake: Wait for Core 0 to acknowledge completion and clear mailbox
     * before re-entering WFE sleep to prevent duplicate execution.
     */
wait_mailbox_clear:
    ldr r1, =secondary_spin_addr
    ldr r2, [r1]
    cmp r2, #0
    bne wait_mailbox_clear

    /* Re-enter low-power WFE loop for next flight frame */
    b secondary_wait_loop

_undef_handler:
    ldr r0, =0x1C090000
    mov r1, #'['
    str r1, [r0]
    mov r1, #'U'
    str r1, [r0]
    mov r1, #'N'
    str r1, [r0]
    mov r1, #'D'
    str r1, [r0]
    mov r1, #'E'
    str r1, [r0]
    mov r1, #'F'
    str r1, [r0]
    mov r1, #']'
    str r1, [r0]
    mov r1, #'\n'
    str r1, [r0]
    b .

_svc_handler:
    b .

_pabt_handler:
    ldr r0, =0x1C090000
    mov r1, #'['
    str r1, [r0]
    mov r1, #'P'
    str r1, [r0]
    mov r1, #'A'
    str r1, [r0]
    mov r1, #'B'
    str r1, [r0]
    mov r1, #'T'
    str r1, [r0]
    mov r1, #']'
    str r1, [r0]
    mov r1, #'\n'
    str r1, [r0]
    b .

_dabt_handler:
    ldr r0, =0x1C090000
    mov r1, #'['
    str r1, [r0]
    mov r1, #'D'
    str r1, [r0]
    mov r1, #'A'
    str r1, [r0]
    mov r1, #'B'
    str r1, [r0]
    mov r1, #'T'
    str r1, [r0]
    mov r1, #']'
    str r1, [r0]
    mov r1, #'\n'
    str r1, [r0]
    b .

_irq_handler:
    subs pc, lr, #4

_fiq_handler:
    subs pc, lr, #4
