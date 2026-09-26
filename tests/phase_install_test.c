/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

#include "phase_install.h"
#include <assert.h>
#include <stdio.h>

static int initial[2], suspend_failure, resume_failure;
static unsigned suspended, restored;
static int restore_irqs[2], restore_states[2];

static int suspend_mock(int irq, int *old)
{
    unsigned i = suspended++;
    assert(i < 2 && irq == install_irqs[i]);
    if ((int)i == suspend_failure)
        return -1;
    *old = initial[i];
    return 0;
}

static int resume_mock(int irq, int old)
{
    assert(restored < 2);
    restore_irqs[restored] = irq;
    restore_states[restored++] = old;
    return irq == resume_failure ? -1 : 0;
}

static void reset(void)
{
    initial[0] = initial[1] = 0;
    suspend_failure = resume_failure = -1;
    suspended = restored = 0;
}

int main(void)
{
    struct install_irq_mask mask;
    reset();
    assert(phase_mask_already_disabled(&mask, suspend_mock, resume_mock) == 0);
    assert(mask.count == 2 && restored == 0);
    assert(install_restore_irqs(&mask, resume_mock) == 0);
    assert(mask.count == 0 && restored == 2);
    assert(restore_irqs[0] == 155 && restore_irqs[1] == 151);
    assert(restore_states[0] == 0 && restore_states[1] == 0);
    for (unsigned i = 0; i < 2; ++i) {
        reset();
        initial[i] = 1;
        assert(phase_mask_already_disabled(&mask, suspend_mock, resume_mock) == -1);
        assert(restored == i + 1 && mask.count == 0);
        assert(restore_irqs[0] == install_irqs[i] && restore_states[0] == 1);
        reset();
        initial[i] = 1;
        resume_failure = install_irqs[i];
        assert(phase_mask_already_disabled(&mask, suspend_mock, resume_mock) == -2);
        assert(restored == i + 1 && mask.count == 0);
        reset();
        suspend_failure = (int)i;
        assert(phase_mask_already_disabled(&mask, suspend_mock, resume_mock) == -2);
        assert(restored == i && mask.count == 0);
        reset();
        resume_failure = install_irqs[i];
        assert(phase_mask_already_disabled(&mask, suspend_mock, resume_mock) == 0);
        assert(install_restore_irqs(&mask, resume_mock) == -1);
        assert(restored == 2 && mask.count == 0);
    }
    puts("PASS: disabled-only installation gate, reverse restoration, failure propagation");
    return 0;
}
