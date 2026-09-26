/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef V2TV_PHASE_INSTALL_H
#define V2TV_PHASE_INSTALL_H

enum { INSTALL_IRQ_COUNT = 2 };
static const int install_irqs[INSTALL_IRQ_COUNT] = {151, 155};
struct install_irq_mask {
    int previous[INSTALL_IRQ_COUNT];
    unsigned count;
};
typedef int (*install_suspend_fn)(int, int *);
typedef int (*install_resume_fn)(int, int);

static int install_restore_irqs(struct install_irq_mask *mask, install_resume_fn resume)
{
    int result = 0;
    while (mask->count) {
        unsigned index = --mask->count;
        if (resume(install_irqs[index], mask->previous[index]) < 0)
            result = -1;
    }
    return result;
}

/* -1: installation rejected with prior known IRQ states restored.
 * -2: IRQ state uncertain; caller must not continue normal boot.
 * A failed suspension has no trustworthy prior-state output. */
static int phase_mask_already_disabled(struct install_irq_mask *mask,
                                      install_suspend_fn suspend,
                                      install_resume_fn resume)
{
    mask->count = 0;
    for (unsigned i = 0; i < INSTALL_IRQ_COUNT; ++i) {
        if (suspend(install_irqs[i], &mask->previous[i]) < 0) {
            (void)install_restore_irqs(mask, resume);
            return -2;
        }
        ++mask->count;
        if (mask->previous[i] != 0)
            return install_restore_irqs(mask, resume) < 0 ? -2 : -1;
    }
    return 0;
}
#endif
