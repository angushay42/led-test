#ifndef RING_BUF_H
#define RING_BUF_H

#include <stdint.h>
#include <stddef.h>

#include "common-defines.h"
#include "error.h"

// this is tying the ringbuffer to my protocol, but that is fine for now.
#define RING_BUF_MAX (1 << 11)      // benchmark this


/*********** ringbuffer  *************/
typedef struct ring_buf_t {
    volatile uint32_t* buffer;    // pointer to buffer with volatile elements
    uint32_t head;      // head index (write)
    uint32_t tail;      // tail index (read)
    uint32_t mask;      // HAS to be (2^n) - 1
} ring_buf_t;


extern error_t ring_buf_setup(ring_buf_t* rb, volatile uint32_t* buffer, const uint32_t size);
extern int ring_buf_empty(ring_buf_t* rb);
extern error_t ring_buf_write(ring_buf_t* rb, uint32_t byte);
extern error_t ring_buf_read(ring_buf_t* rb, volatile uint32_t* byte);

#endif // RING_BUF_H