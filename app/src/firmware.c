#include "common-defines.h"
#include "ringbuffer.h"

#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/timer.h"
#include "libopencm3/stm32/dma.h"
#include "libopencm3/stm32/flash.h"

#include "libopencm3/cm3/systick.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/cm3/cortex.h"

#include <sys/types.h>

volatile uint32_t led_arr, high_duty, low_duty;
volatile uint32_t *encoder[2] = {&low_duty, &high_duty};

#define DATA_SIZE   (3) // bytes
#define LEDS        (12)

// each data packet is 24 bits wide
// 12 leds means we need to store SIZE * LEDS * 8

const uint16_t num_bits = DATA_SIZE * LEDS * 8;
volatile uint16_t pwm_values[DATA_SIZE * LEDS * 8];

enum LED_states {
    send_data,
    send_break
};

enum LED_events {
    data_complete,
    break_complete
};

struct LED_fsm {
    enum LED_states curr_state;
    enum LED_states prev_state;
} fsm;


#define EVENT_QUEUE_SIZE (256)
volatile enum LED_events fsm_events[EVENT_QUEUE_SIZE];
volatile ring_buf_t event_rb;

error_t FSM_get_event() {
    error_t err;
    enum LED_events event;
    ring_buf_read(&event_rb, )

    return OK;
}

error_t FSM_queue_event(struct LED_fsm *fsm) {

}



error_t setup(void);

int main(void) {
    error_t err;
    
    /* general setup from prototyping */
    err = setup();
    if (err) 
        ; // todo handle errors

    /* setup fsm */
    err = ring_buf_setup(&event_rb, fsm_events, EVENT_QUEUE_SIZE);
    if (err) 
        ; // todo handle errors
    
    // initial state 
    fsm.curr_state = send_data;
    err = FSM_get_event(&fsm);
    if (err) 
        ; // todo handle errors
    while (1) {
        switch (fsm.curr_state) {
            case send_data:

                break;
            case send_break:
                break;
            default:
            // todo error here
                break;
        }
        
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
/* current data being transmitted */
volatile data_index = 0;
void dma1_stream0_isr(void) {
    if (dma_get_interrupt_flag(DMA1, DMA_STREAM0, DMA_TCIF)) {
        if (data_index == num_bits - 1) {

        }
        data_index++;
    }
}

error_t setup(void) {
    /* init variables */
    // 84MHz / 800KHz = 105
    led_arr = 105;
    // WS2812B protocol has specific timings for encoding 0s and 1s
    // the line is held high for 0.4us for a bit value of 0 (low)
    // the line is held high for 0.8us for a bit value of 1 (high)
    // the total period of a bit value is 1.25us, so 0.4/1.25 = 32%; 0.8 / 1.25 = 64%
    
    low_duty = (uint32_t) ((float) led_arr * 0.32);
    high_duty = (uint32_t) ((float) led_arr * 0.64);

    /* RCC setup */
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);

    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_TIM4);
    rcc_periph_clock_enable(RCC_DMA1);

    /* DMA setup */
    dma_disable_stream(DMA1, DMA_STREAM0);
    dma_set_transfer_mode(DMA1, DMA_STREAM0, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);
    /* send RGB values from memory to the CCR1 register as a PWM value */
    dma_set_memory_address(DMA1, DMA_STREAM0, &pwm_values);
    dma_set_peripheral_address(DMA1, DMA_STREAM0, TIM4_CCR1);
    /* we are only sending one CCR value at a time */
    dma_set_number_of_data(DMA1, DMA_STREAM0, 1U);
    /* on STM32F4 the channel is 2 for TIM4_CH1 */
    dma_channel_select(DMA1, DMA_STREAM0, DMA_SxCR_CHSEL_2);
    dma_set_priority(DMA1, DMA_STREAM0, DMA_SxCR_PL_VERY_HIGH);
    /* Direct mode does not use the FIFO level, it uses an internal FIFO */
    dma_enable_direct_mode(DMA1, DMA_STREAM0);
    /* todo could this be changed to a byte?*/
    dma_set_memory_size(DMA1, DMA_STREAM0, 16U);
    /* CCR is half word */
    dma_set_peripheral_size(DMA1, DMA_STREAM0, 16U);
    /* manually disable incrementation of CCR */
    dma_disable_peripheral_increment_mode(DMA1, DMA_STREAM0);
    /* enable incrementation of the storage values */
    dma_enable_memory_increment_mode(DMA1, DMA_STREAM0);
    dma_enable_stream(DMA1, DMA_STREAM0);

    
    /* gpio setup */
    // PB6 is TIM4_CH1
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6);
    gpio_set_af(GPIOB, GPIO_AF2, GPIO6);

    /* timer setup */
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
    timer_set_oc_value(TIM4, TIM_OC1, high_duty); 

    // enable interrupt from capture/compare for monitoring
    timer_enable_irq(TIM4, TIM_DIER_CC1IE);
    // disable overflow interrupt
    // the MCU will still set the update flag, but it won't trigger an interrupt.
    timer_disable_irq(TIM4, TIM_DIER_UIE);
    nvic_enable_irq(NVIC_TIM4_IRQ);
    
    timer_enable_oc_output(TIM4, TIM_OC1);
    timer_enable_oc_preload(TIM4, TIM_OC1);
    TIM4_CCMR1 = 0x68;

    // todo check this works
    timer_set_dma_on_update_event(TIM4);
    timer_enable_counter(TIM4);

    return OK;
}
