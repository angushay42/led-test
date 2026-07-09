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



int main(void) {
    error_t err;
    struct FSM_struct fsm;
    unsigned int event, new_state;

    /* initialise clock for whole system */
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);

    if ((err = FSM_init(&fsm, state_init_led)))
        return err;
    
    while (1) {
        /* get next event */
        if ((err = FSM_get_event(&event)))
            return err; // todo

        /* TODO: should this switch statement check previous state? */
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
                return LED_INVALID_EVENT; // todo 
        }
        if (err)
            return err; // todo
        
        
        /* process current state */
        switch (fsm.curr_state) {
            case state_init_led:
                if ((err = LED_init()))
                    return err;
                if ((err = FSM_update_state(&fsm, state_data_start)))
                    return err;
                break;
            case state_data_start:
                LED_start();
                if ((err = FSM_update_state(&fsm, state_data_during)))
                    return err;
                break;
            case state_data_during:
            /* do nothing */
                break;
            case state_data_stop:
                LED_stop();
                if ((err = FSM_update_state(&fsm, state_break_start)))
                    return err;
                break;
            case state_break_start:
                timer_enable_counter(TIM4);
                if ((err = FSM_update_state(&fsm, state_break_during)))
                    return err;
                break;
            case state_break_during:
                /* do nothing */
                break;
            case state_break_stop:
                timer_disable_counter(TIM4);
                timer_set_counter(TIM4, 0);
                break;
            default:
                err = LED_INVALID_STATE; 
                break;
        }
        
    }
    return 0;
}

