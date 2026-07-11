#include "led.h"
#include "fsm.h"
#include "assert.h"

volatile uint32_t led_data_arr, led_break_arr, high_duty, low_duty;
volatile uint32_t *encoder[2] = {&low_duty, &high_duty};

#define NUM_COLOURS   (3) // bytes
#define LEDS        (12)
#define NUM_BITS    (NUM_COLOURS * LEDS * 8)

// each data packet is 24 bits wide
// 12 leds means we need to store SIZE * LEDS * 8

volatile uint16_t pwm_values[NUM_BITS];


/******************* LED Driver *******************/
/* ------------------ ISRs ------------------ */
void tim4_isr(void) {
    timer_clear_flag(TIM4, TIM_SR_UIF);
    timer_clear_flag(TIM4, TIM_SR_CC1IF);

}

void tim3_isr(void) {
    if (timer_get_flag(TIM3, TIM_SR_UIF)) {
        timer_clear_flag(TIM3, TIM_SR_UIF);
        
        // tell other components that break is complete
        FSM_queue_event(event_break_complete);
    }
}

/* current data being transmitted */
volatile int data_index = 0;
void dma1_stream0_isr(void) {
    if (dma_get_interrupt_flag(DMA1, DMA_STREAM0, DMA_TCIF)) {
        dma_clear_interrupt_flags(DMA1, DMA_STREAM0, DMA_TCIF);
        // wait until all data is sent
        if (data_index >= NUM_BITS) {
            data_index++;
            return;
        }
        data_index = 0;
        // stop sending led data
        LED_stop(); 
        // reset memory address
        dma_set_memory_address(DMA1, DMA_STREAM0, (uint32_t) pwm_values);

        // signal to other components 
        FSM_queue_event(event_data_complete);
    }
}

static error_t LED_extract_bits(uint8_t colour, uint16_t *dest) {
    assert(dest != NULL);
    /* copy pointer */
    uint16_t *ptr = dest;
    /* iterate over byte and assign values */
    for (size_t i = 0; i < 8; i++) {
        *ptr++ = *(encoder[(colour & 1)]);
        colour >> 1;
    }
    return OK;
}

/* ------------------ LED API Functions ------------------ */
error_t LED_set_colour(uint8_t r, uint8_t g, uint8_t b) {
    
        
}

error_t LED_start(void) {
    dma_enable_stream(DMA1, DMA_STREAM0);
    timer_set_counter(TIM4, 0U);
    timer_enable_counter(TIM4);
    return OK;
}

error_t LED_stop(void) {
    timer_disable_counter(TIM4);
    dma_disable_stream(DMA1, DMA_STREAM0);
    return OK;
}

/**
 * Configure TIM4 as PWM out
 * 
 */
error_t LED_init(void) {
    // WS2812B protocol has specific timings for encoding 0s and 1s
    // the line is held high for 0.4us for a bit value of 0 (low)
    // the line is held high for 0.8us for a bit value of 1 (high)
    // the total period of a bit value is 1.25us, so 0.4/1.25 = 32%; 0.8 / 1.25 = 64%

    /* init variables */
    // clock frequency is divided by a prescaler and the ARR, Clk / psc = output
    // rearranging to get the desired prescaler: Clk / output = psc
    // therefore: 84MHz / 800KHz = 105
    // this is the frequency of sending data packets
    led_data_arr = 105;

    // 84MHz / (1 / (50us))
    // rearranging the same equation as above, using the break duration, converted to frequency
    led_break_arr = 4200;
    
    low_duty = (uint32_t) ((float) led_data_arr * 0.32);
    high_duty = (uint32_t) ((float) led_data_arr * 0.64);

    for (size_t i = 0; i < NUM_BITS; i++) {
        pwm_values[i] = *(encoder[i & 1]);
    }

    rcc_periph_clock_enable(RCC_TIM4);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_DMA1);

    /* DMA setup */
    dma_disable_stream(DMA1, DMA_STREAM0);
    dma_set_transfer_mode(DMA1, DMA_STREAM0, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);
    /* send RGB values from memory to the CCR1 register as a PWM value */
    dma_set_memory_address(DMA1, DMA_STREAM0, (uint32_t) &pwm_values);
    dma_set_peripheral_address(DMA1, DMA_STREAM0, TIM4_CCR1);
    /* we are only sending one CCR value at a time */
    dma_set_number_of_data(DMA1, DMA_STREAM0, 1U);
    /* on STM32F4 the channel is 2 for TIM4_CH1 */
    dma_channel_select(DMA1, DMA_STREAM0, DMA_SxCR_CHSEL_2);
    dma_set_priority(DMA1, DMA_STREAM0, DMA_SxCR_PL_VERY_HIGH);
    /* Direct mode does not use the FIFO level, it uses an internal FIFO */
    dma_enable_direct_mode(DMA1, DMA_STREAM0);
    //todo could this be changed to a byte?
    dma_set_memory_size(DMA1, DMA_STREAM0, 16U);
    /* CCR is half word */
    dma_set_peripheral_size(DMA1, DMA_STREAM0, 16U);
    /* manually disable incrementation of CCR */
    dma_disable_peripheral_increment_mode(DMA1, DMA_STREAM0);
    /* enable incrementation of the storage values */
    dma_enable_memory_increment_mode(DMA1, DMA_STREAM0);
    
    /* enable interrupt */
    dma_enable_transfer_complete_interrupt(DMA1, DMA_STREAM0);
    nvic_enable_irq(NVIC_DMA1_STREAM0_IRQ);


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
    timer_set_period(TIM4, led_data_arr);
    // with preload enabled, generating an update will force ARR to take the value immediately
    timer_generate_event(TIM4, TIM_EGR_UG);

    timer_set_oc1_mode(TIM4, TIM_CCMR1_OC1M_PWM1);
    // PWM duty cycle is determined by the CCR1 register
    // in LOCM3, set_oc_value writes to this register.
    timer_set_oc_value(TIM4, TIM_OC1, low_duty); 

    /* interrupt config */
    timer_enable_irq(TIM4, TIM_DIER_CC1IE);
    // timer_disable_irq(TIM4, TIM_DIER_CC1IE);
    timer_disable_irq(TIM4, TIM_DIER_UIE);
    nvic_enable_irq(NVIC_TIM4_IRQ);
    
    timer_enable_oc_output(TIM4, TIM_OC1);
    timer_enable_oc_preload(TIM4, TIM_OC1);
    /* manually set it, as it doesn't work */
    TIM4_CCMR1 = 0x68;

    timer_set_dma_on_update_event(TIM4);    // todo check this works

    /* break timer set up */
    rcc_periph_clock_enable(RCC_TIM3);

    timer_disable_counter(TIM3);
    timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_set_prescaler(TIM3, 0U);
    timer_set_period(TIM3, led_break_arr);
    timer_one_shot_mode(TIM3);

    timer_enable_irq(TIM3, TIM_DIER_UIE);
    nvic_enable_irq(NVIC_TIM3_IRQ);

    return OK;
}

error_t LED_teardown(void) {
    dma_stream_reset(DMA1, DMA_STREAM0);

    rcc_periph_clock_disable(RCC_GPIOB);
    rcc_periph_clock_disable(RCC_TIM4);
    rcc_periph_clock_disable(RCC_DMA1);
    return OK;
}