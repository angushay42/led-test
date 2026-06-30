#include "ringbuffer.h"


/* initialise ring buffer. returns 0 if successful */
extern error_t ring_buf_setup(ring_buf_t* rb, uint8_t* buffer, uint32_t size) {
    // check if size is a power of 2
    if (rb == NULL)
        return INVALID_RB_POINTER;
    else if (buffer == NULL) 
        return INVALID_BUFFER_POINTER;
    else if ((size & (size-1)) != 0)
        return INVALID_SIZE;
    
    rb->buffer = buffer;
    rb->mask = size - 1;
    rb->head = 0;
    rb->tail = 0;
    return OK;
}

/* returns 0 if false */
extern int ring_buf_empty(ring_buf_t* rb) {
    return rb->head == rb->tail;    // 0 is false
}

/* write a byte into ring buffer. returns 0 if successful */
extern error_t ring_buf_write(ring_buf_t* rb, uint8_t byte) {
    if (((rb->head + 1) & rb->mask) == rb->tail)  // buffer full
        return RING_BUF_FULL;
    
    rb->buffer[rb->head] = byte;
    rb->head = (rb->head + 1) & rb->mask;
    return OK;   // success
}

/* read a byte from ring buffer. returns 0 if successful */
extern error_t ring_buf_read(ring_buf_t* rb, uint8_t* byte) {
    if (rb->tail == rb->head)
        return RING_BUF_EMPTY;       // err code
    
    // deref and copy byte
    *byte = rb->buffer[rb->tail];

    // increment pointer
    rb->tail = (rb->tail + 1) & rb->mask;   // only works if power of 2.
    return OK;
}