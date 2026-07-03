#ifndef LED_H
#define LED_H

#include "libopencm3/stm32/dma.h"
#include "libopencm3/stm32/timer.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/cm3/cortex.h"
#include "libopencm3/stm32/rcc.h"

#include "common-defines.h"

typedef enum led_errors {
    OK  = 0U,
} led_error_t;

led_error_t LED_start(void);
led_error_t LED_stop(void);
led_error_t LED_init(void);
led_error_t LED_teardown(void);

#endif // LED_H