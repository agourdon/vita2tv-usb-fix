/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

#include "phase_guard.h"
#include "firmware_guard.h"
#include "synthetic_fixture.h"
#include <assert.h>
#include <stdio.h>

static uintptr_t decode(uintptr_t source, const uint8_t b[4])
{
    unsigned a = b[0] | ((unsigned)b[1] << 8);
    unsigned c = b[2] | ((unsigned)b[3] << 8);
    assert((a & 0xf800) == 0xf000 && (c & 0xd000) == 0x9000);
    unsigned s = (a >> 10) & 1;
    unsigned i1 = ~(((c >> 13) & 1) ^ s) & 1;
    unsigned i2 = ~(((c >> 11) & 1) ^ s) & 1;
    uint32_t immediate = (s << 24) | (i1 << 23) | (i2 << 22) |
                         ((a & 0x3ff) << 12) | ((c & 0x7ff) << 1);
    int64_t offset = s ? (int64_t)immediate - 0x2000000 : immediate;
    return (uintptr_t)((int64_t)source + 4 + offset);
}

int main(int argc, char **argv)
{
    assert(argc == 1 || argc == 2);
    unsigned char firmware[UDCD_TEXT_BYTES];
    if (argc == 2) {
        /* Optional private capture; never required or distributed. */
        FILE *f = fopen(argv[1], "rb");
        assert(f);
        assert(fread(firmware, 1, sizeof(firmware), f) == sizeof(firmware));
        assert(fgetc(f) == EOF);
        assert(fclose(f) == 0);
    } else {
        synthetic_fixture(firmware, 0x01cd8000, 0x01c7a000);
    }
    const uintptr_t data = 0x01c7a000;
    assert(phase_guard(firmware, sizeof(firmware), data) == 0);
    assert(phase_guard(NULL, sizeof(firmware), data) < 0);
    assert(phase_guard(firmware, 0x7aaf, data) < 0);
    assert(phase_guard(firmware, sizeof(firmware), data + 4) < 0);
    for (unsigned offset = 0x7a48; offset < 0x7ab0; ++offset) {
        firmware[offset] ^= 1;
        assert(phase_guard(firmware, sizeof(firmware), data) < 0);
        firmware[offset] ^= 1;
    }
    const unsigned anchors[] = {0x76fa, 0x76fe, 0x7702, 0x770a};
    for (unsigned i = 0; i < sizeof(anchors) / sizeof(anchors[0]); ++i)
        for (unsigned j = 0; j < 4; ++j) {
            firmware[anchors[i] + j] ^= 1;
            assert(phase_guard(firmware, sizeof(firmware), data) < 0);
            firmware[anchors[i] + j] ^= 1;
        }
    firmware[PHASE_SKIP] ^= 1;
    assert(phase_guard(firmware, sizeof(firmware), data) < 0);
    firmware[PHASE_SKIP] ^= 1;
    const int32_t offsets[] = {-0x1000000, -4096, -2, 0, 2, 4096, 0xfffffe};
    uint8_t branch[4];
    uintptr_t source = 0x2000000;
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        uintptr_t target = (uintptr_t)((int64_t)source + 4 + offsets[i]);
        assert(phase_branch(source, target | 1, branch) == 0);
        assert(decode(source, branch) == target);
    }
    assert(phase_branch(source, source + 4 + 0x1000000 + 1, branch) < 0);
    assert(phase_branch(source, source + 4 - 0x1000002 + 1, branch) < 0);
    assert(phase_branch(source, source, branch) < 0);
    assert(phase_branch(source | 1, source | 1, branch) < 0);
    assert(phase_branch(source, source | 1, NULL) < 0);
    assert(phase_branch(UINT32_MAX - 2, source | 1, branch) < 0);
    /* Identity checks remain meaningful after injection; the site-specific
     * guard must reject a second installation of an already patched site. */
    assert(guard_firmware(firmware, sizeof(firmware), 0x01cd8000,
                         data, 0x1760) == 0);
    assert(phase_branch(0x01cd8000 + PHASE_SITE, 0x02000001, branch) == 0);
    for (unsigned i = 0; i < 4; ++i)
        firmware[PHASE_SITE + i] = branch[i];
    assert(guard_firmware(firmware, sizeof(firmware), 0x01cd8000,
                         data, 0x1760) == 0);
    assert(phase_guard(firmware, sizeof(firmware), data) < 0);
    const unsigned identity_sites[] = {0x76d0, 0x217c, 0x77a4, 0x7df6,
                                        0xa690, 0xa27c, 0x7b88, 0x7baa};
    for (unsigned i = 0; i < sizeof(identity_sites) / sizeof(identity_sites[0]); ++i) {
        firmware[identity_sites[i]] ^= 1;
        assert(guard_firmware(firmware, sizeof(firmware), 0x01cd8000,
                             data, 0x1760) < 0);
        firmware[identity_sites[i]] ^= 1;
    }
    assert(guard_firmware(firmware, sizeof(firmware) - 1, 0x01cd8000,
                         data, 0x1760) < 0);
    assert(guard_firmware(firmware, sizeof(firmware), 0x01cd8000,
                         data, 0x175f) < 0);
    assert(guard_firmware(firmware, sizeof(firmware), 0x01cd8004,
                         data, 0x1760) < 0);
    puts("PASS: guarded windows, identity mutations, repeat-install rejection and B.W encoding");
    return 0;
}
