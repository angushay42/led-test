#ifndef ERROR_H
#define ERROR_H

#include "libopencm3/stm32/gpio.h"
#include "libopencm3/cm3/cortex.h"
#include "libopencm3/stm32/rcc.h"

#include "common-defines.h"

typedef unsigned int error_t;

enum errors {
    OK = 0U,
    LED_INVALID_EVENT,
    LED_INVALID_STATE,
    RINGBUF_BUFFER_FULL,
    RINGBUF_BUFFER_EMPTY
};
#define ERROR_PORT (GPIOC)
#define ERROR_PIN (GPIO9)

extern void setup_error(void);
extern error_t error_handler(error_t err);

#endif // ERROR_H