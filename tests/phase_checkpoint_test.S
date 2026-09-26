/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

/* CPU-only exhaustive test of the exact candidate assembly. This does not
 * emulate USB registers or establish the meaning of firmware commands.
 * 256 request-type bytes x 5 lengths x 3 NZCVQ/GE patterns = 3840 cases.
 */
.syntax unified
.cpu cortex-a9
.thumb
.text
.global _start
.type _start, %function
.thumb_func
_start:
test_begin:
    movw r0, #:lower16:test_stack_end
    movt r0, #:upper16:test_stack_end
    mov sp, r0
    movw r8, #:lower16:controller_state
    movt r8, #:upper16:controller_state
    movw r0, #:lower16:indices
    movt r0, #:upper16:indices
    ldmia r0, {r4,r5,r7}
    strb.w r4, [r8, #0x544]
    movw r0, #:lower16:lengths
    movt r0, #:upper16:lengths
    ldr.w r5, [r0, r5, lsl #2]
    strh.w r5, [r8, #0x54a]
    movw r6, #:lower16:low_state
    movt r6, #:upper16:low_state
    movw r0, #0x4567
    movt r0, #0x123
    str.w r0, [r6, #0x114]
    movw r1, #:lower16:expected_registers
    movt r1, #:upper16:expected_registers
    str r0, [r1, #12]
    movs r2, #0
    tst r4, #0x60
    beq prepared
    tst r4, #0x80
    bne prepared
    cmp r5, #0
    beq prepared
    movs r2, #1
    /* r6 is the low-state RAM base, whose +0x114 field holds the MMIO base.
     * Suppression must not execute the displaced read through this NULL. */
    movs r6, #0
    movs r0, #0x13
    str r0, [r1, #12]
prepared:
    str r6, [r1, #24]
    str r8, [r1, #32]
    movw r0, #:lower16:expected_skip
    movt r0, #:upper16:expected_skip
    str r2, [r0]
    movw r0, #:lower16:flag_patterns
    movt r0, #:upper16:flag_patterns
    ldr.w r4, [r0, r7, lsl #2]
    movw r0, #:lower16:expected_flags
    movt r0, #:upper16:expected_flags
    str r4, [r0]
    msr apsr_nzcvqg, r4
    mov.w r0, #0x10
    mov.w r1, #0x11
    mov.w r2, #0x12
    mov.w r3, #0x13
    mov.w r4, #0x14
    mov.w r5, #0x15
    mov.w r7, #0x17
    mov.w r9, #0x19
    mov.w r10, #0x1a
    mov.w r11, #0x1b
    mov.w r12, #0x1c
    mov.w lr, #0x1e
    b.w phase_checkpoint
test_resume:
    stmdb.w sp!, {r0-r12, lr}
    mrs r4, apsr
    movs r3, #0
    b test_validate
test_skip:
    stmdb.w sp!, {r0-r12, lr}
    mrs r4, apsr
    movs r3, #1
test_validate:
    movw r0, #:lower16:expected_skip
    movt r0, #:upper16:expected_skip
    ldr r0, [r0]
    cmp r3, r0
    bne fail
    movw r0, #:lower16:test_stack_end
    movt r0, #:upper16:test_stack_end
    sub.w r0, r0, #56
    cmp sp, r0
    bne fail
    movw r1, #:lower16:expected_registers
    movt r1, #:upper16:expected_registers
    movs r2, #14
compare_registers:
    ldr r5, [r0], #4
    ldr r6, [r1], #4
    cmp r5, r6
    bne fail
    subs r2, r2, #1
    bne compare_registers
    movw r0, #0
    movt r0, #0xf80f
    and r4, r4, r0
    movw r1, #:lower16:expected_flags
    movt r1, #:upper16:expected_flags
    ldr r1, [r1]
    and r1, r1, r0
    cmp r4, r1
    bne fail
    movw r0, #:lower16:indices
    movt r0, #:upper16:indices
    ldmia r0, {r1,r2,r3}
    adds r3, r3, #1
    cmp r3, #3
    blo next_case
    movs r3, #0
    adds r2, r2, #1
    cmp r2, #5
    blo next_case
    movs r2, #0
    adds r1, r1, #1
    cmp r1, #256
    beq passed
next_case:
    stmia r0, {r1,r2,r3}
    b test_begin
passed:
    movs r0, #0
    b exit
fail:
    movs r0, #1
exit:
    movs r7, #1
    svc #0
.section .rodata
.align 2
lengths:
    .word 0,1,26,34,65535
flag_patterns:
    .word 0xf80f0000,0x08050000,0
.data
.align 2
.global phase_resume, phase_skip
phase_resume:
    .word test_resume + 1
phase_skip:
    .word test_skip + 1
indices:
    .word 0,0,0
expected_skip:
    .word 0
expected_flags:
    .word 0
expected_registers:
    .word 0x10,0x11,0x12,0x13,0x14,0x15,0,0x17
    .word 0,0x19,0x1a,0x1b,0x1c,0x1e
.bss
.balign 8
controller_state:
    .space 0x550
low_state:
    .space 0x118
.balign 8
test_stack:
    .space 0x1000
test_stack_end:
.section .note.GNU-stack,"",%progbits
