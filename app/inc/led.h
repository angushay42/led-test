#ifndef LED_H
#define LED_H

#include "libopencm3/stm32/dma.h"
#include "libopencm3/stm32/timer.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/cm3/cortex.h"
#include "libopencm3/stm32/rcc.h"

#include "common-defines.h"
#include "error.h"

struct pixel {
    uint8_t g;
    uint8_t r;
    uint8_t b;
};

void LED_set_pixel(struct pixel *pxl, size_t pos);


error_t LED_init(void);
error_t LED_teardown(void);

error_t LED_start(void);
error_t LED_stop(void);

enum LED_states {
    state_init_led,
    state_data_start,
    state_data_during,
    state_data_stop,
    state_break_start,
    state_break_during,
    state_break_stop
};

enum LED_events {
    event_none,
    event_init_complete,
    event_data_complete,
    event_break_complete
};

#endif // LED_H