#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/timer.h"
#include "libopencm3/stm32/dma.h"
#include "libopencm3/stm32/flash.h"

#include "libopencm3/cm3/systick.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/cm3/cortex.h"

#include "common-defines.h"
#include "fsm.h"
#include "led.h"
#include "error.h"


extern void delay_ms(uint32_t ms) {
    for (size_t i = 0; i < (size_t) ((double) ms * 84000000.0 / 1000 / 7.0); i++ )
        __asm__("NOP");
}

static void test_setup(void);

static void test_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOC);

    gpio_mode_setup(
        GPIOC, 
        GPIO_MODE_OUTPUT,
        GPIO_PUPD_PULLDOWN,
        GPIO8 | GPIO7 | GPIO6 | GPIO5
    );
    gpio_clear(
        GPIOC,
        GPIO8 | GPIO7 | GPIO6 | GPIO5
    );
}


int main(void) {
    error_t err;
    struct FSM_struct fsm;
    unsigned int event, new_state;

    /* initialise clock for whole system */
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);

    test_setup();
    setup_error();

    if ((err = FSM_init(&fsm, state_init_led)))
        return error_handler(err);
    
    while (1) {
        /* get next event */
        if ((err = FSM_get_event(&event)))
            return error_handler(err);

        /* change state if needed */
        switch (event) {
            case event_none:
                __asm__("NOP");
                break;
            case event_init_complete:
                err = FSM_update_state(&fsm, state_data_start);
                break;
            case event_data_complete:

                err = FSM_update_state(&fsm, state_break_start);
                break;
            case event_break_complete:
                err = FSM_update_state(&fsm, state_data_start);
                break;
            default:
                return error_handler(LED_INVALID_EVENT);
        }
        if (err)
            return error_handler(err);

        /* process current state */
        switch (fsm.curr_state) {
            case state_init_led:
                if ((err = LED_init()))
                    return error_handler(err);
                // todo why do this when i have created event_init complete? do one or the other.
                if ((err = FSM_update_state(&fsm, state_data_start)))
                    return error_handler(err);

                break;
            case state_data_start:
                LED_start();
                if ((err = FSM_update_state(&fsm, state_data_during)))
                    return error_handler(err);

                break;
            case state_data_during:
            /* do nothing */
                break;
            case state_data_stop:
                LED_stop();
                if ((err = FSM_update_state(&fsm, state_break_start)))
                    return error_handler(err);

                break;
            case state_break_start:
                timer_enable_counter(TIM3);
                if ((err = FSM_update_state(&fsm, state_break_during)))
                    return error_handler(err);
                break;
            case state_break_during:
                /* do nothing */
                break;
            case state_break_stop:
                /* do nothing, one-pulse mode will automate this*/
                break;
            default:
                return error_handler(LED_INVALID_STATE); 
        }
        
    }
    return 0;
}

