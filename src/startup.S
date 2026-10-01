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
    b _start            /* 0x00 Reset */
    b _undef_handler    /* 0x04 Undefined Instruction */
    b _svc_handler      /* 0x08 Software Interrupt / SVC */
    b _pabt_handler     /* 0x0C Prefetch Abort */
    b _dabt_handler     /* 0x10 Data Abort */
    b _reserved_handler /* 0x14 Reserved */
    b _irq_handler      /* 0x18 IRQ */
    b _fiq_handler      /* 0x1C FIQ */

.section .text
.align 2

_start:
    /* Set Vector Base Address Register (VBAR) */
    ldr r0, =_vector_table
    mcr p15, 0, r0, c12, c0, 0
    isb

    /* Clear SCTLR.V (bit 13) to ensure low/VBAR vector mapping */
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #(1 << 13)
    mcr p15, 0, r0, c1, c0, 0
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
     * 32KB total stack: 8KB (8192 bytes) partition per core.
     * Core partition base = _stack_top - (core_id * 8192)
     */
    ldr r1, =_stack_top
    mov r2, #8192
    mul r3, r0, r2
    sub r1, r1, r3              /* r1 = Core partition stack top */
    mov sp, r1                  /* SVC stack top (6KB available) */

    /* Initialize banked exception stacks for this core */
    cps #0x17                   /* Abort mode */
    sub sp, r1, #6144
    cps #0x1B                   /* Undefined mode */
    sub sp, r1, #6656
    cps #0x12                   /* IRQ mode */
    sub sp, r1, #7168
    cps #0x11                   /* FIQ mode */
    sub sp, r1, #7680
    cps #0x13                   /* Return to Supervisor mode */

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
    dmb
    ldr r1, =secondary_spin_addr
    ldr r2, [r1]
    cmp r2, #0
    beq secondary_wait_loop
    dmb

    /* Branch to task entry point */
    blx r2

    /*
     * Handshake: Wait for Core 0 to acknowledge completion and clear mailbox
     * before re-entering WFE sleep to prevent duplicate execution.
     * Uses WFE so secondary core rests in low-power state until Core 0 issues SEV.
     */
wait_mailbox_clear:
    dmb
    ldr r1, =secondary_spin_addr
    ldr r2, [r1]
    cmp r2, #0
    beq 1f
    wfe
    b wait_mailbox_clear
1:
    dmb
    /* Re-enter low-power WFE loop for next flight frame */
    b secondary_wait_loop

_undef_handler:
    sub lr, lr, #4
    sub sp, sp, #32
    mrc p15, 0, r0, c0, c0, 5
    lsr r1, r0, #8
    and r1, r1, #0xFF
    and r0, r0, #0xFF
    add r1, r0, r1, lsl #2
    str r1, [sp, #0]
    mov r0, #1                  /* EXC_TYPE_UNDEF */
    str r0, [sp, #4]
    str lr, [sp, #8]
    mrs r2, spsr
    str r2, [sp, #12]
    mov r0, #0
    str r0, [sp, #16]
    str r0, [sp, #20]
    mov r0, sp
    bl handle_core_exception
1:  wfe
    b 1b

_svc_handler:
    sub sp, sp, #32
    mrc p15, 0, r0, c0, c0, 5
    lsr r1, r0, #8
    and r1, r1, #0xFF
    and r0, r0, #0xFF
    add r1, r0, r1, lsl #2
    str r1, [sp, #0]
    mov r0, #2                  /* EXC_TYPE_SVC */
    str r0, [sp, #4]
    str lr, [sp, #8]
    mrs r2, spsr
    str r2, [sp, #12]
    mov r0, #0
    str r0, [sp, #16]
    str r0, [sp, #20]
    mov r0, sp
    bl handle_core_exception
1:  wfe
    b 1b

_pabt_handler:
    sub lr, lr, #4
    sub sp, sp, #32
    mrc p15, 0, r0, c0, c0, 5
    lsr r1, r0, #8
    and r1, r1, #0xFF
    and r0, r0, #0xFF
    add r1, r0, r1, lsl #2
    str r1, [sp, #0]
    mov r0, #3                  /* EXC_TYPE_PABT */
    str r0, [sp, #4]
    str lr, [sp, #8]
    mrs r2, spsr
    str r2, [sp, #12]
    mrc p15, 0, r2, c5, c0, 1   /* IFSR */
    str r2, [sp, #16]
    mrc p15, 0, r2, c6, c0, 2   /* IFAR */
    str r2, [sp, #20]
    mov r0, sp
    bl handle_core_exception
1:  wfe
    b 1b

_dabt_handler:
    sub lr, lr, #8
    sub sp, sp, #32
    mrc p15, 0, r0, c0, c0, 5
    lsr r1, r0, #8
    and r1, r1, #0xFF
    and r0, r0, #0xFF
    add r1, r0, r1, lsl #2
    str r1, [sp, #0]
    mov r0, #4                  /* EXC_TYPE_DABT */
    str r0, [sp, #4]
    str lr, [sp, #8]
    mrs r2, spsr
    str r2, [sp, #12]
    mrc p15, 0, r2, c5, c0, 0   /* DFSR */
    str r2, [sp, #16]
    mrc p15, 0, r2, c6, c0, 0   /* DFAR */
    str r2, [sp, #20]
    mov r0, sp
    bl handle_core_exception
1:  wfe
    b 1b

_reserved_handler:
1:  wfe
    b 1b

_irq_handler:
    sub lr, lr, #4
    sub sp, sp, #32
    mrc p15, 0, r0, c0, c0, 5
    lsr r1, r0, #8
    and r1, r1, #0xFF
    and r0, r0, #0xFF
    add r1, r0, r1, lsl #2
    str r1, [sp, #0]
    mov r0, #5                  /* EXC_TYPE_IRQ */
    str r0, [sp, #4]
    str lr, [sp, #8]
    mrs r2, spsr
    str r2, [sp, #12]
    mov r0, #0
    str r0, [sp, #16]
    str r0, [sp, #20]
    mov r0, sp
    bl handle_core_exception
1:  wfe
    b 1b

_fiq_handler:
    sub lr, lr, #4
    sub sp, sp, #32
    mrc p15, 0, r0, c0, c0, 5
    lsr r1, r0, #8
    and r1, r1, #0xFF
    and r0, r0, #0xFF
    add r1, r0, r1, lsl #2
    str r1, [sp, #0]
    mov r0, #6                  /* EXC_TYPE_FIQ */
    str r0, [sp, #4]
    str lr, [sp, #8]
    mrs r2, spsr
    str r2, [sp, #12]
    mov r0, #0
    str r0, [sp, #16]
    str r0, [sp, #20]
    mov r0, sp
    bl handle_core_exception
1:  wfe
    b 1b
