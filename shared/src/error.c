#include "error.h"

extern void setup_error(void) {
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_mode_setup(ERROR_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, ERROR_PIN);
}


extern error_t error_handler(error_t err) {
    cm_disable_interrupts();

    gpio_clear(ERROR_PORT, ERROR_PIN);
    for (;;) {
        for (size_t i = 0; i < (size_t) err; i++) {
            gpio_set(ERROR_PORT, ERROR_PIN);
            delay_ms(100);
            gpio_clear(ERROR_PORT, ERROR_PIN);
            delay_ms(200);
        }
        delay_ms(1500);
    }
}