#ifndef ERROR_H
#define ERROR_H

typedef unsigned int error_t;

enum errors {
    OK = 0U,
    LED_INVALID_EVENT,
    LED_INVALID_STATE,
    RINGBUF_BUFFER_FULL,
    RINGBUF_BUFFER_EMPTY
};

extern void handle_error(error_t err);

#endif // ERROR_H