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
    setup();
    while (1) {
        ;
    }
    return 0;
}
void tim4_isr(void) {
    // timer_clear_flag(TIM4, TIM_SR_CC1IF); // todo remove for production
    timer_clear_flag(TIM4, TIM_SR_UIF);     // todo remove for production

    if (timer_get_flag(TIM4, TIM_SR_CC1IF)) {
        timer_clear_flag(TIM4, TIM_SR_CC1IF);
        gpio_set(GPIOB, GPIO10);
        gpio_clear(GPIOB, GPIO10);
    }
    // if(timer_get_flag(TIM4, TIM_SR_UIF)){
    //     timer_clear_flag(TIM4, TIM_SR_UIF);
    //     gpio_toggle(GPIOB, GPIO10);
    // }
}

error_t setup(void) {
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);

    // set up a timer to output a pwm signal with 1.25us period or 800KHz
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_TIM4);

    // todo remove for production pin used for testing
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GPIO10);
    gpio_clear(GPIOB, GPIO10);
    
    // PB6 is TIM4_CH1
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6);
    gpio_set_af(GPIOB, GPIO_AF2, GPIO6);

    uint32_t led_arr, high_duty, low_duty;
    
    // 84MHz / 800KHz = 105
    led_arr = 105;
    // WS2812B protocol has specific timings for encoding 0s and 1s
    // the line is held high for 0.4us for a bit value of 0 (low)
    // the line is held high for 0.8us for a bit value of 1 (high)
    // the total period of a bit value is 1.25us, so 0.4/1.25 = 32%; 0.8 / 1.25 = 64%
    
    low_duty = (uint32_t) ((float) led_arr * 0.32);
    high_duty = (uint32_t) ((float) led_arr * 0.64);

    // disable before configuration
    timer_disable_counter(TIM4);

    timer_continuous_mode(TIM4);
    timer_enable_preload(TIM4);
    // upcounting, no clock division, edge alignment
    timer_set_mode(TIM4, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

    // keep frequency at 84MHz
    timer_set_prescaler(TIM4, 0);   
    // PWM frequency is determined by the ARR register, which set_period writes to
    timer_set_period(TIM4, led_arr);
    // with preload enabled, generating an update will force ARR to take the value immediately
    timer_generate_event(TIM4, TIM_EGR_UG);

    timer_set_oc1_mode(TIM4, TIM_CCMR1_OC1M_PWM1);
    // PWM duty cycle is determined by the CCR1 register
    // in LOCM3, set_oc_value writes to this register.
    timer_set_oc_value(TIM4, TIM_OC1, low_duty); 

    // enable interrupt from capture/compare for monitoring
    timer_enable_irq(TIM4, TIM_DIER_CC1IE);
    // disable overflow interrupt
    // the MCU will still set the update flag, but it won't trigger an interrupt.
    timer_disable_irq(TIM4, TIM_DIER_UIE);
    nvic_enable_irq(NVIC_TIM4_IRQ);
    
    timer_enable_oc_output(TIM4, TIM_OC1);
    timer_enable_oc_preload(TIM4, TIM_OC1);
    TIM4_CCMR1 = 0x68;

    timer_enable_counter(TIM4);

    return OK;
}
