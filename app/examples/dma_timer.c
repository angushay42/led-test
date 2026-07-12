#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/timer.h"
#include "libopencm3/stm32/dma.h"
#include "libopencm3/stm32/flash.h"

#include "libopencm3/cm3/systick.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/cm3/cortex.h"

#include "common-defines.h"


#define TEST_PORT   (GPIOC)
#define DMA_ENTERED_PIN     (GPIO9)
#define DMA_COMPLETE_PIN     (GPIO8)
#define TIM4_UPDATE_PIN     (GPIO6)
#define DMA_HALF_PIN    (GPIO5)
#define ERROR_PIN           (GPIO7)

/* test values for DMA verification */
volatile uint16_t values[2] = {1 << 4, 1 << 8};


enum errors {
    OK,
    TIM_FIRST_VALUE_NE,
    TIM_SECOND_VALUE_NE,
    DMA_MEM_NE,
    DMA_PERIPH_NE,
    DMA_EN_UNSET,
    DMA_TIM_SECOND_VALUE_NE
};

// todo maybe a void * for extra data?
static void error_handler(int err);
static void dma_setup(void);
static void data_timer_setup(void);
static void delay_ms(int ms);

static void delay_ms(int ms) {
    for (size_t i = 0; i < (size_t) ((double) ms * 84000000.0 / 1000 / 7.0); i++ )
        __asm__("NOP");
}   

static void error_handler(int err) {
    cm_disable_interrupts();

    gpio_clear(TEST_PORT, ERROR_PIN);
    for (;;) {
        for (size_t i = 0; i < (size_t) err; i++) {
            gpio_set(TEST_PORT, ERROR_PIN);
            delay_ms(100);
            gpio_clear(TEST_PORT, ERROR_PIN);
            delay_ms(200);
        }
        delay_ms(1500);
    }
}

static void dma_setup(void) {
    /** 
     * Desired affect:
     * The dma will cycle between two values at each timer update.
     * Timer counts up and once ARR is reached, a new value is placed into ARR and thus a different
     * period.
     * 
     * How this can be achieved:
     * dma has 2 data to send. 
     * in direct mode, each request causes a transfer.
     * in circular mode, once all data are sent, the address restores and starts again
     */


    /* clock the peripheral */
    rcc_periph_clock_enable(RCC_DMA1);


    /* disable before configuration */
    dma_disable_stream(DMA1, DMA_STREAM6);
    dma_stream_reset(DMA1, DMA_STREAM6);

    /* send RGB values from memory to the CCR1 register as a PWM value */
    dma_set_transfer_mode(DMA1, DMA_STREAM6, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);
    
    dma_set_memory_address(DMA1, DMA_STREAM6, (uint32_t) &values[0]);
    if ((uint32_t) DMA_SM0AR(DMA1, DMA_STREAM6) != (uint32_t) (&values[0]))
        error_handler(DMA_MEM_NE); 

    dma_set_peripheral_address(DMA1, DMA_STREAM6, (uint32_t) &TIM4_ARR);
    if ((uint32_t) DMA_SPAR(DMA1, DMA_STREAM6) != (uint32_t) &TIM4_ARR)
        error_handler(DMA_PERIPH_NE); 

    /* we are only sending one CCR value at a time */
    dma_set_number_of_data(DMA1, DMA_STREAM6, 2U);

    /* TIM4_CH1 is stream 0, channel 2 on STM32F4 */
    dma_channel_select(DMA1, DMA_STREAM6, DMA_SxCR_CHSEL_2);
    dma_set_priority(DMA1, DMA_STREAM6, DMA_SxCR_PL_VERY_HIGH);

    dma_enable_circular_mode(DMA1, DMA_STREAM6);
    dma_enable_direct_mode(DMA1, DMA_STREAM6);

    dma_set_memory_size(DMA1, DMA_STREAM6, DMA_SxCR_MSIZE_16BIT); // todo could be reduced to a byte ?
    /* CCR is half word */
    dma_set_peripheral_size(DMA1, DMA_STREAM6, DMA_SxCR_PSIZE_16BIT);

    /* manually disable incrementation of CCR */
    dma_disable_peripheral_increment_mode(DMA1, DMA_STREAM6);

    dma_enable_memory_increment_mode(DMA1, DMA_STREAM6);
    
    /* enable interrupt */
    dma_enable_transfer_complete_interrupt(DMA1, DMA_STREAM6);
    dma_enable_half_transfer_interrupt(DMA1, DMA_STREAM6);

    nvic_enable_irq(NVIC_DMA1_STREAM6_IRQ);
    
    dma_enable_stream(DMA1, DMA_STREAM6);
    if (!(DMA1_S6CR & DMA_SxCR_EN))
        error_handler(DMA_EN_UNSET);
}

static void data_timer_setup(void) {
    /* clock both the timer AND the GPIO port */
    rcc_periph_clock_enable(RCC_TIM4);
    rcc_periph_clock_enable(RCC_GPIOB);

    // PB6 is TIM4_CH1
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6);
    /* af2 is timer channels */
    gpio_set_af(GPIOB, GPIO_AF2, GPIO6);

    /* disable before configuration */
    timer_disable_counter(TIM4);

    timer_continuous_mode(TIM4);
    // timer_enable_preload(TIM4);
    // upcounting, no clock division, edge alignment
    timer_set_mode(TIM4, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

    // keep frequency at 84MHz
    timer_set_prescaler(TIM4, 4200);   

    timer_set_period(TIM4, (uint32_t) values[0]);
    
    // with preload enabled, generating an update will force ARR to take the value immediately
    timer_generate_event(TIM4, TIM_EGR_UG);
    timer_clear_flag(TIM4, TIM_SR_UIF);

    /** OC configuration */
    /* set to PWM1 mode */
    // timer_set_oc1_mode(TIM4, TIM_CCMR1_OC1M_ACTIVE); 
    // PWM duty cycle is determined by the CCR1 register
    // in LOCM3, set_oc_value writes to this register.
    // timer_set_oc_value(TIM4, TIM_OC1, values[0]);

    // timer_enable_oc_output(TIM4, TIM_OC1);
    // timer_enable_oc_preload(TIM4, TIM_OC1);
    /* manually set it, as it doesn't work */
    // if (TIM4_CCMR1 != 0x68)
    //     TIM4_CCMR1 = 0x68;

    /* interrupt config */
    // timer_enable_irq(TIM4, TIM_DIER_CC1IE);
    timer_disable_irq(TIM4, TIM_DIER_CC1IE);
    timer_enable_irq(TIM4, TIM_DIER_UIE);
    timer_enable_irq(TIM4, TIM_DIER_UDE);

    nvic_enable_irq(NVIC_TIM4_IRQ);

    /* send dma request on every update */
    timer_set_dma_on_update_event(TIM4);
}



void dma1_stream6_isr(void) {
    gpio_toggle(TEST_PORT, DMA_ENTERED_PIN);

    /* transfer complete */
    if (dma_get_interrupt_flag(DMA1, DMA_STREAM6, DMA_TCIF)) {
        dma_clear_interrupt_flags(DMA1, DMA_STREAM6, DMA_TCIF);
        gpio_toggle(TEST_PORT, DMA_COMPLETE_PIN);
    }
    /* half transfer complete */
    else if (dma_get_interrupt_flag(DMA1, DMA_STREAM6, DMA_HTIF)) {
        dma_clear_interrupt_flags(DMA1, DMA_STREAM6, DMA_HTIF);
        gpio_toggle(TEST_PORT, DMA_HALF_PIN);
    }
}

volatile size_t count = 0;
volatile uint32_t limit = 10;
void tim4_isr(void) {
    // volatile uint32_t scr = DMA_SCR(DMA1, DMA_STREAM6);
    // volatile uint32_t ndtr = DMA_SNDTR(DMA1, DMA_STREAM6);
    // volatile uint32_t hisr = DMA1_HISR;   // stream6 flags live in HISR
    // timer_clear_flag(TIM4, TIM_SR_UIF);
    // timer_clear_flag(TIM4, TIM_SR_CC1IF);
    
    if (timer_get_flag(TIM4, TIM_SR_UIF)) {
        gpio_toggle(TEST_PORT, TIM4_UPDATE_PIN);
        /* first value */
        if (!(count & 1)) {
            if (TIM4_ARR != values[0])
                error_handler(TIM_FIRST_VALUE_NE);
        }
        else {
            if (TIM4_ARR != values[1] && count >= limit)
                error_handler(TIM_SECOND_VALUE_NE);
        }
        count++;
        timer_clear_flag(TIM4, TIM_SR_UIF);
    }
    // if (timer_get_flag(TIM4, TIM_SR_CC1IF)) {
    //     timer_clear_flag(TIM4, TIM_SR_CC1IF);
    //     gpio_toggle(TEST_PORT, TIM4_COMPARE_PIN);
    // }
}

int main(void) {
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);
    
    // test setup
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_mode_setup(TEST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, 
        GPIO9 | GPIO8 | GPIO6 | GPIO5 | GPIO7);

    gpio_clear(TEST_PORT, GPIO9 | GPIO8 | GPIO6 | GPIO5 | GPIO7); 

    dma_setup();
    data_timer_setup();

    timer_enable_counter(TIM4);

    for (;;) {
        /* do nothing */
    }
}
