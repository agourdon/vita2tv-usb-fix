/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

#include "phase_model.h"

enum {
    USB_DIRECTION_IN = 0x80,
    USB_REQUEST_TYPE_MASK = 0x60,
};

bool original_early_command(struct control_request request)
{
    /* Firmware post-dispatch selection at text offset 0x7a8e. */
    return (request.request & 0xf5u) == 1u || request.request == 5u;
}

bool candidate_early_command(struct control_request request)
{
    bool nonstandard = (request.request_type & USB_REQUEST_TYPE_MASK) != 0;
    bool output = (request.request_type & USB_DIRECTION_IN) == 0;
    bool has_data = request.length != 0;

    /* Suppress only the early command for non-standard OUT data requests.
     * All other original dispatch decisions remain unchanged. */
    return original_early_command(request) &&
           !(nonstandard && output && has_data);
}

struct command_schedule original_schedule(struct control_request request,
                                          bool full_receive_completed)
{
    return (struct command_schedule) {
        .after_dispatch = original_early_command(request),
        .after_successful_receive = full_receive_completed,
    };
}

struct command_schedule candidate_schedule(struct control_request request,
                                           bool full_receive_completed)
{
    return (struct command_schedule) {
        .after_dispatch = candidate_early_command(request),
        /* Firmware's normal successful receive path remains untouched.
         * This external observation is never inferred from partial data. */
        .after_successful_receive = full_receive_completed,
    };
}
