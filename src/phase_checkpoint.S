/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

/* EP0 phase-order trampoline, entered through B.W at firmware offset 0x7aa0.
 * No calls, logging, FIFO accesses, or synthetic receive completion.
 * The displaced LDR executes once on pass-through and never on suppression.
 * Both continuation pointers must be published before patch activation.
 */
.syntax unified
.cpu cortex-a9
.thumb
.text
.align 2
.global phase_checkpoint
.type phase_checkpoint, %function
.thumb_func
phase_checkpoint:
    sub sp, sp, #32
    stmia.w sp, {r0-r3, r12, lr}
    mrs r12, apsr
    str r12, [sp, #24]
    ldrb.w r0, [r8, #0x544]
    tst.w r0, #0x60
    beq phase_original
    tst.w r0, #0x80
    bne phase_original
    ldrh.w r1, [r8, #0x54a]
    cbz r1, phase_original
    movw r12, #:lower16:phase_skip
    movt r12, #:upper16:phase_skip
    b phase_return
phase_original:
    ldr.w r3, [r6, #0x114]
    str r3, [sp, #12]
    movw r12, #:lower16:phase_resume
    movt r12, #:upper16:phase_resume
phase_return:
    ldr r12, [r12]
    str r12, [sp, #28]
    ldr r12, [sp, #24]
    msr apsr_nzcvqg, r12
    ldmia.w sp!, {r0-r3, r12, lr}
    add.w sp, sp, #4
    pop {pc}
.size phase_checkpoint, .-phase_checkpoint
.section .note.GNU-stack,"",%progbits
