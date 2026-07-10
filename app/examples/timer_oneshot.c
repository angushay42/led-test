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

#include "libopencm3/stm32/exti.h"
#include "math.h"
#include "libopencm3/stm32/usart.h"
#include "libopencm3/stm32/syscfg.h"

static void delay_ms(double ms) {
    size_t i, n;

    // 7 gives 99 ms 
    for (i = 0, n = (size_t) round(ms * 84000000.0 / 1000.0 / 7.0); i < n; i++)
        __asm volatile ("NOP");
}

void tim3_isr(void) {
    if (timer_get_flag(TIM3, TIM_SR_UIF)) {
        timer_clear_flag(TIM3, TIM_SR_UIF);
        
        gpio_clear(GPIOB, GPIO6);
        // // turn TIM3 off
        // timer_disable_counter(TIM3);

        // tell other components that break is complete
        // FSM_queue_event(event_break_complete);
    }
}

void exti4_isr(void) {
    /* flag_status is 1 if triggered by selected trigger, 0 otherwise */
    if (exti_get_flag_status(EXTI4)) {
        
        delay_ms(20);
        if (gpio_get(GPIOB, GPIO4)) {
            gpio_set(GPIOB, GPIO6);
            timer_clear_flag(TIM3, TIM_SR_CC1IF);
            timer_enable_counter(TIM3); 
        }
    }
    // clear flag
    exti_reset_request(EXTI4);
}



int main(void) {
    // 84MHz / 20KHz = 4200
    uint32_t break_arr = 4200;
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);

    rcc_periph_clock_enable(RCC_TIM3);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART2);
    rcc_periph_clock_enable(RCC_SYSCFG);

    
    /* test gpio */
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GPIO6);
    gpio_clear(GPIOB, GPIO6);
    /* button port */
    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO4);

    /* uart tx  */
    gpio_set_af(GPIOA, GPIO_AF7, GPIO2);
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO2);
    
    timer_disable_counter(TIM3);
    timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_set_prescaler(TIM3, 0U);
    timer_one_shot_mode(TIM3);
    timer_set_oc_value(TIM3, TIM_OC1, break_arr - 1);

    // CCR1 is the delay, ARR - CCR1 is the pulse duration
    timer_set_period(TIM3, break_arr); // 84MHz / (1/50us)

    timer_enable_irq(TIM3, TIM_DIER_UIE);
    nvic_enable_irq(NVIC_TIM3_IRQ);

    /* uart */
    usart_disable(USART2);
    usart_set_baudrate(USART2, 115200);
    usart_set_stopbits(USART2, USART_STOPBITS_1);
    usart_set_databits(USART2, 8);
    usart_set_parity(USART2, USART_PARITY_NONE);
    usart_set_mode(USART2, USART_MODE_TX);
    usart_enable(USART2);

    
    /* button */
    exti_set_trigger(EXTI4, EXTI_TRIGGER_RISING);
    exti_enable_request(EXTI4);
    exti_select_source(EXTI4, GPIOB);
    nvic_enable_irq(NVIC_EXTI4_IRQ);
    exti_reset_request(EXTI4);
    
    uint16_t val;
    while (1) {
        // delay_ms(500);
        // val = gpio_get(GPIOB, GPIO4);
        // if (val)
        //     gpio_toggle(GPIOB, GPIO6);
    }
    return 0;
}
