#ifndef FSM_H
#define FSM_H

#include "libopencm3/cm3/cortex.h"

#include "common-defines.h"
#include "ringbuffer.h"
#include "error.h"

struct FSM_struct {
    unsigned int curr_state;
    unsigned int prev_state;
};

error_t FSM_init(struct FSM_struct *fsm);
error_t FSM_update_state(struct FSM_struct *fsm);
error_t FSM_get_event(unsigned int *event);
error_t FSM_queue_event(unsigned int event);

#endif // FSM_H