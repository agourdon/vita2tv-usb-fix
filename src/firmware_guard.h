/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef V2TV_USB_FIX_FIRMWARE_GUARD_H
#define V2TV_USB_FIX_FIRMWARE_GUARD_H

#include <stddef.h>
#include <stdint.h>

enum {
    UDCD_TEXT_BYTES = 0xb104, UDCD_DATA_BYTES = 0x1760,
    UDCD_IRQ_OFFSET = 0x76d0, UDCD_IRQ_MAP = 0xa690,
    UDCD_IRQ_FIRST = 151, UDCD_IRQ_MAP_BYTES = 39,
};

static uint16_t guard_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static int guard_move(const uint8_t *p, unsigned high, unsigned reg,
                       uint16_t *value)
{
    uint16_t a = guard_u16(p), b = guard_u16(p + 2);
    if ((a & 0xfbf0) != (high ? 0xf2c0 : 0xf240) ||
        (b & 0x8000) != 0 || ((b >> 8) & 15) != reg)
        return -1;
    *value = (uint16_t)(((a & 15) << 12) | (((a >> 10) & 1) << 11) |
                         (((b >> 12) & 7) << 8) | (b & 255));
    return 0;
}

static int guard_address(const uint8_t *low, const uint8_t *high,
                          unsigned reg, uint32_t *value)
{
    uint16_t a, b;
    if (guard_move(low, 0, reg, &a) || guard_move(high, 1, reg, &b))
        return -1;
    *value = a | ((uint32_t)b << 16);
    return 0;
}

/* Verify code identity without assuming boot-specific relocated addresses.
 * These are entry/branch bytes and RAM anchors from the captured 3.65 module,
 * not universal Vita SDK offsets. Reject any mismatch before installing the patch. */
static int guard_firmware(const uint8_t *text, size_t text_bytes,
                           uintptr_t text_base, uintptr_t data_base,
                           size_t data_bytes)
{
    static const uint8_t irq[] = {
        0x2d,0xe9,0xf0,0x4f,0x49,0xf6,0x2c,0x74,
        0xc0,0xf2,0xae,0x04,0x97,0x38,0x26,0x28,
        0x85,0xb0,0x23,0x68,0x03,0x93,0x00,0xf2,
        0x4f,0x81,0x42,0xf2,0x90,0x63,0xc0,0xf2,
        0xce,0x13,0x1d,0x56,0x00,0x2d,0x40,0xf0,
    };
    static const uint8_t retirement[] = {
        0x2d,0xe9,0xf0,0x47,0x4a,0xf2,0x00,0x05,
        0x44,0x69,0x98,0x46,0xc0,0xf2,0xc7,0x15,
        0x40,0xf2,0x0c,0x63,0x07,0x46,0x89,0x46,
        0x92,0x46,0x03,0xfb,0x08,0x55,0x5c,0xb3,
        0x26,0x6a,0x00,0x2e,0x5d,0xd0,0x46,0x61,
    };
    static const uint8_t fifo_loop[] = {
        0x8e,0x45,0x40,0xf2,0x26,0x83,0xd6,0xf8,0x14,0xc1,
        0x04,0xe0,0xdc,0xf8,0x38,0x02,0x00,0x28,0x40,0xf0,
        0x02,0x82,0xdc,0xf8,0x38,0x00,0x80,0x07,0xf6,0xd0,
    };
    static const uint8_t completion[] = {
        0x91,0x61,0x18,0x46,0x02,0xaa,0x00,0x21,0x2b,0x46,
        0xfa,0xf7,0xb4,0xf9,0xd6,0xf8,0x14,0x01,0x29,0x46,
        0x01,0x22,0xfd,0xf7,0xbe,0xfe,0xd7,0xe4,
    };
    static const int8_t map[] = {
        1,1,1,1,2,2,2,2,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0,0,
        0,-1,-1,-1,-1,-1,0,
    };
    if (!text || text_bytes != UDCD_TEXT_BYTES ||
        data_bytes != UDCD_DATA_BYTES || !text_base || !data_base ||
        (text_base & 3) || (data_base & 3) ||
        text_base > UINT32_MAX - UDCD_TEXT_BYTES ||
        data_base > UINT32_MAX - UDCD_DATA_BYTES)
        return -1;
    for (unsigned i = 0; i < sizeof(irq); ++i) {
        if ((i >= 4 && i < 12) || (i >= 26 && i < 34))
            continue;
        if (text[UDCD_IRQ_OFFSET + i] != irq[i])
            return -1;
    }
    for (unsigned i = 0; i < sizeof(retirement); ++i) {
        if ((i >= 4 && i < 8) || (i >= 12 && i < 16))
            continue;
        if (text[0x216c + i] != retirement[i])
            return -1;
    }
    for (unsigned i = 0; i < sizeof(fifo_loop); ++i)
        if (text[0x77a4 + i] != fifo_loop[i])
            return -1;
    for (unsigned i = 0; i < sizeof(completion); ++i)
        if (text[0x7df6 + i] != completion[i])
            return -1;
    for (unsigned i = 0; i < sizeof(map); ++i)
        if ((int8_t)text[UDCD_IRQ_MAP + i] != map[i])
            return -1;
    /* The original IRQ uses this exact time export via its 0x969c stub.
     * Its NID and known calls remain conservative code-identity anchors.
     * This correction does not resolve or call it from the interrupt handler. */
    static const uint8_t time_nid[] = {0x49,0xde,0xf6,0x47};
    static const uint8_t first_time_call[] = {0x01,0xf0,0x88,0xed};
    static const uint8_t next_time_call[] = {0x01,0xf0,0x78,0xed};
    for (unsigned i = 0; i < 4; ++i)
        if (text[0xa27c + i] != time_nid[i] ||
            text[0x7b88 + i] != first_time_call[i] ||
            text[0x7baa + i] != next_time_call[i])
            return -1;
    uint32_t stack_guard, irq_map, controller_base;
    if (guard_address(text + UDCD_IRQ_OFFSET + 4,
                       text + UDCD_IRQ_OFFSET + 8, 4, &stack_guard) ||
        guard_address(text + UDCD_IRQ_OFFSET + 26,
                       text + UDCD_IRQ_OFFSET + 30, 3, &irq_map) ||
        guard_address(text + 0x2170, text + 0x2178, 5, &controller_base))
        return -1;
    if (!stack_guard || (stack_guard & 3) ||
        irq_map != text_base + UDCD_IRQ_MAP || controller_base != data_base)
        return -1;
    return 0;
}

#endif
