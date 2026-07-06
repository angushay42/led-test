#include "ringbuffer.h"
#include "assert.h"

/* initialise ring buffer. returns 0 if successful */
extern error_t ring_buf_setup(ring_buf_t* rb, volatile uint32_t* buffer, uint32_t size) {
    assert(rb != NULL);
    assert(buffer != NULL);
    // check if size is a power of 2
    assert((size & (size-1)) == 0);
    
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
extern error_t ring_buf_write(ring_buf_t* rb, uint32_t byte) {
    /* if writing would cause pointers to be equal, then the buffer is full */
    if (((rb->head + 1) & rb->mask) == rb->tail)
        return RINGBUF_BUFFER_FULL;
    
    rb->buffer[rb->head] = byte;
    rb->head = (rb->head + 1) & rb->mask;
    return OK;
}

/* read a byte from ring buffer. returns 0 if successful */
extern error_t ring_buf_read(ring_buf_t* rb, volatile uint32_t* byte) {
    /* buffer empty if pointers are at same location */
    if (rb->tail == rb->head)
        return RINGBUF_BUFFER_EMPTY;
    
    // deref and copy byte
    *byte = rb->buffer[rb->tail];

    // increment pointer
    rb->tail = (rb->tail + 1) & rb->mask;   // only works if power of 2.
    return OK;
}