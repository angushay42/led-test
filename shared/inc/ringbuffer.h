#ifndef RING_BUF_H
#define RING_BUF_H

#include "common-defines.h"
#include <stdint.h>
#include <stddef.h>

// this is tying the ringbuffer to my protocol, but that is fine for now.
#define RING_BUF_MAX (1 << 11)      // benchmark this


/*********** ringbuffer  *************/
typedef struct ring_buf_t {
    uint32_t* buffer;    // pointer to buffer
    uint32_t head;      // head index (write)
    uint32_t tail;      // tail index (read)
    uint32_t mask;      // HAS to be (2^n) - 1
} ring_buf_t;

typedef enum ring_buf_errors {
    OK = 0U,
    INVALID_RB_POINTER,
    INVALID_BUFFER_POINTER,
    INVALID_SIZE,
    RING_BUF_FULL,
    RING_BUF_EMPTY
} ring_buf_error_t;

extern error_t ring_buf_setup(ring_buf_t* rb, uint32_t* buffer, uint32_t size);
extern int ring_buf_empty(ring_buf_t* rb);
extern error_t ring_buf_write(ring_buf_t* rb, uint32_t byte);
extern error_t ring_buf_read(ring_buf_t* rb, uint32_t* byte);

#endif // RING_BUF_H