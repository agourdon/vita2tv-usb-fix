/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef V2TV_EP0_PHASE_MODEL_H
#define V2TV_EP0_PHASE_MODEL_H

#include <stdbool.h>
#include <stdint.h>

/* Offline model of command-1 scheduling only. No register semantics are
 * assigned to the command, and no receive completion is synthesized. */
struct control_request {
    uint8_t request_type;
    uint8_t request;
    uint16_t length;
};

struct command_schedule {
    bool after_dispatch;
    bool after_successful_receive;
};

bool original_early_command(struct control_request request);
bool candidate_early_command(struct control_request request);
struct command_schedule original_schedule(struct control_request request,
                                          bool full_receive_completed);
struct command_schedule candidate_schedule(struct control_request request,
                                           bool full_receive_completed);

#endif
