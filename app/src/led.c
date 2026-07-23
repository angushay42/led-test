#include "led.h"
#include "fsm.h"
#include "assert.h"

volatile uint32_t led_data_arr, led_break_arr, high_duty, low_duty;
volatile uint16_t encoder[2];

#define NUM_COLOURS     (3) // bytes
#define NUM_LEDS        (16)
#define NUM_BITS    (NUM_COLOURS * NUM_LEDS * 8)
#define LITTLE_ENDIAN   (true)

// each data packet is 24 bits wide
// 12 leds means we need to store SIZE * LEDS * 8

volatile uint16_t pwm_values[NUM_BITS];

/* static prototypes */
static void dma_setup(void);
static void data_timer_setup(void);
static void break_timer_setup(void);
static void LED_map_colour(uint8_t colour, volatile uint16_t *dest, bool little);


/* ------------------ ISRs ------------------ */
void tim4_isr(void) {
    if (timer_get_flag(TIM4, TIM_SR_UIF)) {
        timer_clear_flag(TIM4, TIM_SR_UIF);
        
        /* disable this again */
        timer_disable_irq(TIM4, TIM_DIER_UIE);

        LED_stop();

        // signal to other components 
        FSM_queue_event(event_data_complete);
    }
    timer_clear_flag(TIM4, TIM_SR_CC1IF);

}

void tim3_isr(void) {
    if (timer_get_flag(TIM3, TIM_SR_UIF)) {
        timer_clear_flag(TIM3, TIM_SR_UIF);
        
        // tell other components that break is complete
        FSM_queue_event(event_break_complete);
    }
}

// todo this shouldn't stop the timer, the timer should update one last time
void dma1_stream6_isr(void) {
    /* transfer is complete once NDTR is 0 (all data sent, circular buffer reloaded) */
    if (dma_get_interrupt_flag(DMA1, DMA_STREAM6, DMA_TCIF)) {
        dma_clear_interrupt_flags(DMA1, DMA_STREAM6, DMA_TCIF);
        timer_enable_irq(TIM4, TIM_DIER_UIE);
    }
}


/* ------------------ LED API Functions ------------------ */

error_t LED_start(void) {
    timer_set_counter(TIM4, 0U);
    timer_clear_flag(TIM4, TIM_SR_UIF);
    // dma_set_memory_address(DMA1, DMA_STREAM6, (uint32_t) &pwm_values);
    // dma_set_number_of_data(DMA1, DMA_STREAM6, NUM_BITS);
    // timer_set_oc_value(DMA1, DMA_STREAM6, pwm_values[0]);
    timer_enable_counter(TIM4);
    timer_set_oc1_mode(TIM4, TIM_OCM_PWM1);
    return OK;
}

error_t LED_stop(void) {
    timer_disable_counter(TIM4);
    /* manually reset stream */
    
    timer_set_oc1_mode(TIM4, TIM_OCM_FORCE_LOW);
    
    return OK;
}

static void dma_setup(void) {
    /* clock the peripheral */
    rcc_periph_clock_enable(RCC_DMA1);

    /* disable before configuration */
    dma_disable_stream(DMA1, DMA_STREAM6);
    dma_stream_reset(DMA1, DMA_STREAM6);

    /* send RGB values from memory to the CCR1 register as a PWM value */
    dma_set_transfer_mode(DMA1, DMA_STREAM6, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);
    dma_set_memory_address(DMA1, DMA_STREAM6, (uint32_t) &pwm_values);
    dma_set_peripheral_address(DMA1, DMA_STREAM6, (uint32_t) &TIM4_CCR1);

    /* total data to be sent is NUM_BITS */
    dma_set_number_of_data(DMA1, DMA_STREAM6, NUM_BITS);

    /* on STM32F4 the channel is 2 for TIM4_UP */
    dma_channel_select(DMA1, DMA_STREAM6, DMA_SxCR_CHSEL_2);
    dma_set_priority(DMA1, DMA_STREAM6, DMA_SxCR_PL_VERY_HIGH);
    /* Direct mode does not use the FIFO level, it uses an internal FIFO */
    dma_enable_direct_mode(DMA1, DMA_STREAM6);
    /* circular mode will reset memory pointer upon full transfer */
    dma_enable_circular_mode(DMA1, DMA_STREAM6);

    dma_set_memory_size(DMA1, DMA_STREAM6, DMA_SxCR_MSIZE_16BIT); // todo could be reduced to a byte ?
    /* CCR is half word */
    dma_set_peripheral_size(DMA1, DMA_STREAM6, DMA_SxCR_PSIZE_16BIT);
    /* manually disable incrementation of CCR */
    dma_disable_peripheral_increment_mode(DMA1, DMA_STREAM6);
    /* enable incrementation of the storage values */
    dma_enable_memory_increment_mode(DMA1, DMA_STREAM6);
    
    /* enable interrupt */
    dma_enable_transfer_complete_interrupt(DMA1, DMA_STREAM6);
    nvic_enable_irq(NVIC_DMA1_STREAM6_IRQ);

    dma_enable_stream(DMA1, DMA_STREAM6);
}

static void data_timer_setup(void) {
    /* clock both the timer AND the GPIO port */
    rcc_periph_clock_enable(RCC_TIM4);
    rcc_periph_clock_enable(RCC_GPIOB);

    // PB6 is TIM4_CH1
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6);
    // gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_100MHZ, GPIO6);
    
    /* af2 is timer channels */
    gpio_set_af(GPIOB, GPIO_AF2, GPIO6);

    /* disable before configuration */
    timer_disable_counter(TIM4);

    timer_continuous_mode(TIM4);

    // upcounting, no clock division, edge alignment
    timer_set_mode(TIM4, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

    // keep frequency at 84MHz
    timer_set_prescaler(TIM4, 0);   
    // PWM frequency is determined by the ARR register, which set_period writes to
    timer_set_period(TIM4, led_data_arr);

    /* force low initially, to prevent unwanted */
    timer_set_oc1_mode(TIM4, TIM_OCM_FORCE_LOW);

    // PWM duty cycle is determined by the CCR1 register
    // in LOCM3, set_oc_value writes to this register.
    timer_set_oc_value(TIM4, TIM_OC1, low_duty); 

    /* interrupt config */
    /* IMPORTANT: enable the DMA with THIS flag */
    timer_enable_irq(TIM4, TIM_DIER_UDE);

    /* todo don't need interrupts so far. */
    timer_disable_irq(TIM4, TIM_DIER_CC1IE);
    timer_disable_irq(TIM4, TIM_DIER_UIE);
    nvic_enable_irq(NVIC_TIM4_IRQ);
    
    timer_enable_oc_output(TIM4, TIM_OC1);
    timer_disable_oc_preload(TIM4, TIM_OC1);
    /* manually set it, as it doesn't work */
    TIM4_CCMR1 = 0x68;

    /* send dma request on every update */
    timer_set_dma_on_update_event(TIM4);
}

static void break_timer_setup(void) {
    /* clock the timer */
    rcc_periph_clock_enable(RCC_TIM3);

    /* disable first */
    timer_disable_counter(TIM3);
    /* upcounting, no division */
    timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_set_prescaler(TIM3, 0U);

    /* value should be configured before this */
    timer_set_period(TIM3, led_break_arr);
    /* one-shot mode will disable and reset the timer after each update */
    timer_one_shot_mode(TIM3);

    /* enable interrupt to send event to FSM */
    timer_enable_irq(TIM3, TIM_DIER_UIE);
    nvic_enable_irq(NVIC_TIM3_IRQ);
}

static void LED_map_colour(uint8_t colour, volatile uint16_t *dest, bool little) {
    assert(dest != NULL);

    // copy pointer
    volatile uint16_t *ptr = dest;
    for (volatile size_t i= 0; i < 8; i++) {
        /* fill dest in reverse order */
        /* if the LSB is set, the index to encoder will be 1 */
        if (!little)
            *(ptr + i) = encoder[colour & 1];
        else
            *(ptr + 8 - 1 - i) = encoder[colour & 1];
        colour >>= 1;
    }
}

/**
 * @param pos the 1-indexed position of an LED
 */
void LED_set_pixel(struct pixel *pxl, size_t pos) {
    assert(pxl != NULL); 
    assert(pos > 0 && pos <= NUM_LEDS);

    /* copy pointer */
    volatile uint16_t *ptr = pwm_values + (uint16_t) ((pos - 1) * NUM_COLOURS * 8);

    /* fill bits in GRB order (as per WS2812B) */    
    LED_map_colour(pxl->g, ptr, LITTLE_ENDIAN);
    LED_map_colour(pxl->r, (ptr+8), LITTLE_ENDIAN);
    LED_map_colour(pxl->b, ptr+16, LITTLE_ENDIAN);
}

/**
 * Configure TIM4 as PWM out
 * 
 * @details 
 * WS2812B protocol has specific timings for encoding 0s and 1s
 * the line is held high for 0.4us for a bit value of 0 (low)
 * the line is held high for 0.8us for a bit value of 1 (high)
 * the total period of a bit value is 1.25us, so 0.4/1.25 = 32%; 0.8 / 1.25 = 64%
 */
error_t LED_init(void) {
    /* 84MHz / 800KHz = 105 */
    led_data_arr = 105;

    // 84MHz / (1 / (50us))
    // rearranging the same equation as above, using the break duration, converted to frequency
    led_break_arr = 25200;
    
    low_duty = (uint32_t) ((float) led_data_arr * 0.32);
    high_duty = (uint32_t) ((float) led_data_arr * 0.64);

    /* initialise encoder */
    encoder[0] = (uint16_t) low_duty;
    encoder[1] = (uint16_t) high_duty;

    struct pixel pxl = {
        .g = 0,
        .r = 0,
        .b = 10
    };

    for (size_t i = 0; i < NUM_BITS; i++)
        pwm_values[i] = 0;

    /* fill initial values for the LED colours */
    for (size_t i = 0; i < NUM_LEDS; i++) {
        // if (i > 7) {
        //     pxl.r <<= 1;
        //     pxl.r |= 1;
        // }
        // pxl.g <<= 1;
        // pxl.g |= 1;
        
        LED_set_pixel(&pxl, i+1);
    }
    // for (size_t i = 0; i < NUM_BITS; i++) {
        
    // }

    dma_setup();
    data_timer_setup();
    break_timer_setup();

    return OK;
}

error_t LED_teardown(void) {
    dma_stream_reset(DMA1, DMA_STREAM6);

    rcc_periph_clock_disable(RCC_GPIOB);
    rcc_periph_clock_disable(RCC_TIM4);
    rcc_periph_clock_disable(RCC_DMA1);
    return OK;
}