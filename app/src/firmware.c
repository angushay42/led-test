#include "common-defines.h"

#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/timer.h"
#include "libopencm3/stm32/dma.h"
#include "libopencm3/stm32/flash.h"

#include "libopencm3/cm3/systick.h"
#include "libopencm3/cm3/nvic.h"

#include <sys/types.h>




error_t setup(void);



int main(void) {
    while (1) {
        ;
    }
    return 0;
}

error_t setup(void) {
    return OK;
}
