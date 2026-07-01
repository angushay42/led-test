#ifndef FSM_H
#define FSM_H

#include "common-defines.h"
#include "ringbuffer.h"

enum LED_states {
    state_none,
    state_send_data,
    state_send_break
};

enum LED_events {
    event_none,
    event_data_complete,
    event_break_complete
};

struct LED_fsm {
    enum LED_states curr_state;
    enum LED_states prev_state;
};

typedef enum FSM_errors {
    OK = 0U,
    INVALID_EVENT,
    INVALID_STATE,
    NULL_FSM_POINTER,
} FSM_error_t;

#define EVENT_QUEUE_SIZE (256)

volatile enum LED_events fsm_events[EVENT_QUEUE_SIZE];
volatile ring_buf_t event_rb;

#endif // FSM_H