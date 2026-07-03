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


int main(void) {
    error_t err;
    struct LED_fsm fsm;

    /* initialise clock for whole system */
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);

    if ((err = FSM_init(&fsm)))
        return err;
    
    while (1) {
        /* get current state */
        if ((err = FSM_update_state(&fsm)))
            return err; // todo
        
        if (fsm.curr_state == fsm.prev_state)
            continue;
        
        switch (fsm.curr_state) {
            case state_send_data:
                err = FSM_handle_send_data(&fsm);
                break;
            case state_send_break:
                err = FSM_handle_send_break(&fsm);
                break;
            default:
                err = INVALID_STATE;
                break;
        }
        if (err) 
            return err; // todo
        
    }
    return 0;
}

