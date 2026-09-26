/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

#include "phase_model.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static bool standard_out_id(uint8_t request)
{
    /* Independent enumeration of the original numeric selection. */
    return request == 1 || request == 3 || request == 5 ||
           request == 9 || request == 11;
}

int main(void)
{
    const uint16_t lengths[] = { 0, 1, 26, 34, UINT16_MAX };
    size_t cases = 0, original_early = 0, candidate_early = 0, suppressed = 0;

    for (unsigned type = 0; type <= UINT8_MAX; ++type) {
        for (unsigned id = 0; id <= UINT8_MAX; ++id) {
            for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
                struct control_request request = {
                    .request_type = (uint8_t)type,
                    .request = (uint8_t)id,
                    .length = lengths[i],
                };
                bool selected = standard_out_id(request.request);
                bool excluded = (type & 0x60u) != 0 &&
                                (type & 0x80u) == 0 && lengths[i] > 0;
                bool original = original_early_command(request);
                bool candidate = candidate_early_command(request);

                assert(original == selected);
                assert(candidate == (selected && !excluded));
                assert(!candidate || original); /* Never add an early command. */
                if (!excluded)
                    assert(candidate == original);

                /* Compare both schedules with and without an independently
                 * observed full receive completion. A partial/cancelled
                 * receive must not be turned into successful completion. */
                for (unsigned completed = 0; completed < 2; ++completed) {
                    struct command_schedule before =
                        original_schedule(request, completed != 0);
                    struct command_schedule after =
                        candidate_schedule(request, completed != 0);
                    assert(before.after_dispatch == original);
                    assert(after.after_dispatch == candidate);
                    assert(before.after_successful_receive == (completed != 0));
                    assert(after.after_successful_receive ==
                           before.after_successful_receive);
                }

                ++cases;
                original_early += original;
                candidate_early += candidate;
                suppressed += original && !candidate;
            }
        }
    }

    assert(cases == 327680);
    assert(original_early == 6400);
    assert(candidate_early == 4480);
    assert(suppressed == 1920);
    printf("PASS: %zu requests; early command original=%zu candidate=%zu "
           "suppressed=%zu; successful-receive scheduling unchanged\n",
           cases, original_early, candidate_early, suppressed);
    return 0;
}
