#include "fsm.h"
#include "assert.h"
#include "led.h"

#define EVENT_QUEUE_SIZE (256)

volatile uint32_t fsm_events[EVENT_QUEUE_SIZE];
ring_buf_t event_rb;

/******************* FSM API *******************/

/* create event queue and set initial state for a FSM struct */
error_t FSM_init(
    struct FSM_struct *fsm, 
    unsigned int initial_state) {
    error_t err;

    assert(fsm != NULL);

    /* set up queue */
    if ((err = ring_buf_setup(&event_rb, fsm_events, EVENT_QUEUE_SIZE)))
        return err;
    
    /* initial state is send data */
    (*fsm).curr_state = initial_state;
    (*fsm).prev_state = (unsigned int) -1;  //todo ok?
    return OK;
}

/* read from event queue and update state if event exists */
error_t FSM_update_state(struct FSM_struct *fsm) {
    error_t err;
    enum LED_events event;
    enum LED_states state;

    assert (fsm != NULL);

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
            return FSM_INVALID_STATE;
    }
    /* save previous state and update current */
    (*fsm).prev_state = (*fsm).curr_state;
    (*fsm).curr_state = state;

    return OK;
}

/* get next event from queue, thread-safe */
error_t FSM_get_event(unsigned int *event) {
    error_t err;
    volatile uint32_t temp;
    
    /* enter critical section */
    cm_disable_interrupts();
    
    /* copy shared value into local variable */
    err = ring_buf_read(&event_rb, &temp);
    
    /* exit critical section */
    cm_enable_interrupts(); 

    /* todo this is tightly coupled */
    if (err == RINGBUF_BUFFER_EMPTY)
        *event = event_none;
    else 
        *event = (enum LED_events) temp;
    
    return OK;
}

/* add event to the FSM queue, thread-safe */
error_t FSM_queue_event(unsigned int event) {
    error_t err;

    /* enter critical section */
    cm_disable_interrupts();

    err = ring_buf_write(&event_rb, (volatile uint32_t) event);

    /* exit critical section */
    cm_enable_interrupts();

    return err;
}

// /*  */
// error_t FSM_state_handler(struct LED_fsm *fsm, unsigned int (*handler)()) {

// }