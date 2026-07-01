#include "fsm.h"

/******************* FSM API *******************/

/* create event queue and set initial state for a FSM struct */
error_t FSM_init(struct LED_fsm *fsm) {
    error_t err;
    if (fsm == NULL)
        return NULL_FSM_POINTER;

    /* set up queue */
    if ((err = ring_buf_setup(&event_rb, fsm_events, EVENT_QUEUE_SIZE)))
        return err;
    
    /* initial state is send data */
    (*fsm).curr_state = state_send_data;
    (*fsm).prev_state = state_none;
    return OK;
}

/* read from event queue and update state if event exists */
error_t FSM_update_state(struct LED_fsm *fsm) {
    error_t err;
    enum LED_events event;
    enum LED_states state;

    if (fsm == NULL)
        return NULL_FSM_POINTER;

    /* consume event from queue */
    if ((err = FSM_get_event(&event)))
        return err;
    
    /* process event */
    switch (event) {
        /* if no event, do nothing */
        case event_none:
            return OK;
        
        /* data complete moves fsm to send break state */
        case event_data_complete:
            state = state_send_break;
            break;

        /* break complete moves fsm to send data */
        case event_break_complete:
            state = state_send_data;
            break; 

        default:
            return INVALID_EVENT;
    }
    /* save previous state and update current */
    (*fsm).prev_state = (*fsm).curr_state;
    (*fsm).curr_state = state;

    return OK;
}

/* get next event from queue, thread-safe */
error_t FSM_get_event(enum LED_events *event) {
    error_t err;
    
    /* enter critical section */
    cm_disable_interrupts();
    
    /* copy shared value into local variable */
    err = ring_buf_read(&event_rb, event);
    
    /* exit critical section */
    cm_enable_interrupts(); 

    /* todo this is tightly coupled */
    if (err == RING_BUF_EMPTY)
        *event = event_none;
    
    return OK;
}

/* add event to the FSM queue, thread-safe */
error_t FSM_queue_event(enum LED_events event) {
    error_t err;

    /* enter critical section */
    cm_disable_interrupts();

    err = ring_buf_write(&event_rb, event);

    /* exit critical section */
    cm_enable_interrupts();

    return err;
}

error_t FSM_handle_send_data(struct LED_fsm *fsm) {
    error_t err;
    if ((*fsm).prev_state == state_none)
        err = LED_setup();
    

}
error_t FSM_handle_send_break(struct LED_fsm *fsm) {

}