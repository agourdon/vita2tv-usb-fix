/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef V2TV_PHASE_GUARD_H
#define V2TV_PHASE_GUARD_H

#include <stddef.h>
#include <stdint.h>

enum { PHASE_SITE = 0x7aa0, PHASE_RESUME = 0x7aa4, PHASE_SKIP = 0x77dc };

/* Specific to the archived 3.65 module. Guard setup dispatch, the complete
 * numeric selection, displaced LDR and both original continuations.
 * There is no enclosing IT block or branch to the second halfword of LDR. */
static int phase_guard(const uint8_t *text, size_t length, uintptr_t data_base)
{
    static const uint8_t window[] = {
        0xc3,0xf8,0x0c,0x02,0x01,0xf0,0x0e,0xee,
        0x02,0xa8,0x29,0x46,0xf9,0xf7,0x80,0xfd,
        0x00,0x28,0xc0,0xf2,0x4b,0x81,0x98,0xf8,
        0x45,0x35,0x03,0xf0,0xf5,0x02,0x01,0x2a,
        0x02,0xd0,0x05,0x2b,0x7f,0xf4,0x9e,0xae,
        0xd6,0xf8,0x14,0x31,0x01,0x22,0xc3,0xf8,
        0x0c,0x22,0x01,0xf0,0xf8,0xed,0x95,0xe6,
    };
    static const uint8_t setup[] = {
        0xd3,0xf8,0x10,0x12,0x6f,0xf3,0xc7,0x12,
        0x32,0x72,0x0a,0x0a,0x88,0xf8,0x45,0x25,
        0xd3,0xf8,0x10,0x22,0x88,0xf8,0x44,0x25,
        0xd3,0xf8,0x14,0xe2,0xd3,0xf8,0x18,0x12,
        0xd3,0xf8,0x1c,0x22,0xa8,0xf8,0x46,0xe5,
        0xa8,0xf8,0x48,0x15,0xa8,0xf8,0x4a,0x25,
    };
    static const uint8_t stride[] = {0x40,0xf2,0x0c,0x68};
    static const uint8_t construct[] = {0x08,0xfb,0x05,0x38};
    if (!text || length < 0x7a78 + sizeof(window) || !data_base ||
        (data_base & 3) || data_base > UINT32_MAX - 0x1760)
        return -1;
    for (size_t i = 0; i < sizeof(window); ++i)
        if (text[0x7a78 + i] != window[i])
            return -1;
    for (size_t i = 0; i < sizeof(setup); ++i)
        if (text[0x7a48 + i] != setup[i])
            return -1;
    for (size_t i = 0; i < sizeof(stride); ++i)
        if (text[0x76fe + i] != stride[i] || text[0x770a + i] != construct[i])
            return -1;
    /* MOVW/MOVT r3 build this boot's data base; MLA then constructs r8 as
     * data + controller_index * 0x60c. Validate opcodes and relocations. */
    uint32_t anchor = 0;
    for (unsigned high = 0; high < 2; ++high) {
        const uint8_t *p = text + (high ? 0x7702 : 0x76fa);
        uint16_t a = p[0] | ((uint16_t)p[1] << 8);
        uint16_t b = p[2] | ((uint16_t)p[3] << 8);
        if ((a & 0xfbf0) != (high ? 0xf2c0 : 0xf240) ||
            (b & 0x8000) || ((b >> 8) & 15) != 3)
            return -1;
        uint32_t value = ((a & 15) << 12) | (((a >> 10) & 1) << 11) |
                         (((b >> 12) & 7) << 8) | (b & 255);
        anchor |= value << (high * 16);
    }
    if (anchor != data_base)
        return -1;
    return text[PHASE_SKIP] == 0xbb && text[PHASE_SKIP + 1] == 0x07 ? 0 : -1;
}

/* Encode a non-linking Thumb-2 B.W; LR belongs to the interrupted firmware. */
static int phase_branch(uintptr_t source, uintptr_t target, uint8_t bytes[4])
{
    if (!bytes || source > UINT32_MAX - 4 || (source & 1) ||
        target > UINT32_MAX || !(target & 1))
        return -1;
    int64_t distance = (int64_t)(target & ~(uintptr_t)1) - (source + 4);
    if ((distance & 1) || distance < -0x1000000 || distance > 0xfffffe)
        return -1;
    uint32_t d = (uint32_t)distance;
    unsigned s = (d >> 24) & 1, i1 = (d >> 23) & 1, i2 = (d >> 22) & 1;
    unsigned j1 = (~(i1 ^ s)) & 1, j2 = (~(i2 ^ s)) & 1;
    uint16_t first = 0xf000 | (s << 10) | ((d >> 12) & 0x3ff);
    uint16_t second = 0x9000 | (j1 << 13) | (j2 << 11) | ((d >> 1) & 0x7ff);
    bytes[0] = (uint8_t)first;
    bytes[1] = (uint8_t)(first >> 8);
    bytes[2] = (uint8_t)second;
    bytes[3] = (uint8_t)(second >> 8);
    return 0;
}
#endif
