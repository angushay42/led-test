/**
 * commands:
 *  health status
 *  start 
 *  stop
 *  set colour
 *  get logs
 */

#include "uartp.h"

static uint8_t _buffer[RING_BUF_MAX];
static ring_buf_t _rb;
uint8_t count = 0;

volatile bool packet_found = false;
static bool packet_started = false;

/* @todo added volatile to prevent optimising out?*/
/* private timestamp of last time packet was polled*/
static volatile uint64_t last_poll_packet = 0;

/* static prototypes */
static void uartp_enable(void);
static void uartp_disable(void);
static uart_error_t uartp_send(struct packet *p);
static uart_error_t uartp_send_8(struct packet *p);
static uart_error_t uartp_send_16(struct packet *p);
static uart_error_t uartp_send_32(struct packet *p);
static uart_error_t uartp_send_64(struct packet *p);
static uart_error_t uartp_validate_packet(uint8_t buffer[], bool *found);
static uint8_t get_size_from_flag(uint8_t flag);
static uart_error_t uartp_fill_packet(struct packet *p, uint8_t buffer[]);
static uart_error_t uartp_fill_8(struct packet *p, uint8_t buffer[], size_t len);


/** @TODO
 * it could be beneficial to "flatten" data I want to send and then just cycle it out.
 * i.e. each packet gets flattened to an identical format of bytes to be sent over uartp?
 */
static uart_error_t uartp_send_8(struct packet *p) {
    uint8_t *ptr, temp;
    ptr = (*p).u;
    for (size_t i = 0; i < (*p).len; i++) {
        temp = *(ptr + i);
        usart_send_blocking(UART, (uint8_t) temp);
    }
    return OK;
}

static uart_error_t uartp_send_16(struct packet *p) {
    uint16_t *ptr, temp;
    ptr = (*p).u;
    for (size_t i = 0; i < (*p).len; i++) {
        temp = *(ptr + i);
        for (size_t j = 0; j < (*p).size; j++) {
            usart_send_blocking(UART, (uint8_t) temp);
            temp >>= 8;
        }
    }
    return OK;
}

static uart_error_t uartp_send_32(struct packet *p) {
    uint32_t *ptr, temp;
    ptr = (*p).u;
    for (size_t i = 0; i < (*p).len; i++) {
        temp = *(ptr + i);
        for (size_t j = 0; j < (*p).size; j++) {
            usart_send_blocking(UART, (uint8_t) temp);
            temp >>= 8;
        }
    }
    return OK;
}

static uart_error_t uartp_send_64(struct packet *p) {
    uint64_t *ptr, temp;
    ptr = (*p).u;
    for (size_t i = 0; i < (*p).len; i++) {
        temp = *(ptr + i);
        for (size_t j = 0; j < (*p).size; j++) {
            usart_send_blocking(UART, (uint8_t) temp);
            temp >>= 8;
        }
    }
    return OK;
}

extern uart_error_t uartp_send_packet(struct packet *p) {
    uart_error_t err;
    size_t n;

    if (p == NULL)
        return PACKET_NULL_POINTER;

    if ((*p).id == NULL)
        return PACKET_NULL_ID;
    
    /** create packet for string identifier and send it */
    /* get length of id */
    for (n = 0; (*p).id[n] != '\0'; n++)
        ;
    // todo
    if (n > (size_t) 255)
        return PACKET_LENGTH_INVALID;

    
    struct packet ch = {
        .len = n,
        .size = 1,
        .is_signed = false,
        .u = (*p).id,
    };

    // send id
    if ((err = uartp_send(&ch)))
        return err;

    // send data
    if ((err = uartp_send(p)))
        return err;
    return OK;
}

// todo wish this was more general...
static uart_error_t uartp_send(struct packet *p) {
    if (p == NULL)
        return PACKET_NULL_POINTER;

    uint8_t flag;

    // send start char
    usart_send_blocking(UART, (uint8_t) '{');

    flag = (1 << ((*p).size / 2));
    if ((*p).is_signed)
        flag |= (1 << fsigned);

    // send metadata in flag
    usart_send_blocking(UART, (uint8_t) flag);

    // send length of data to come
    usart_send_blocking(UART, (uint16_t) (*p).len);

    switch ((*p).size) {
        case 1:
            uartp_send_8(p);
            break;
        case 2:
            uartp_send_16(p);
            break;
        case 4:
            uartp_send_32(p);
            break;
        case 8:
            uartp_send_64(p);
            break;
        default:
            return PACKET_WORD_SIZE_INVALID;
    }
    
    // send stop char
    usart_send_blocking(UART, (uint8_t) '}');

    return OK;
}

/* @ todo CHECK FOR NULL POINTERs */
extern uart_error_t uartp_poll_packet(struct packet *p, uint64_t poll_period, bool *found) {
    if (p == NULL)
        return PACKET_NULL_POINTER;
    if ((*p).u == NULL)
        return PACKET_NULL_DATA_POINTER;
    uart_error_t err;
    /* get current time */
    uint64_t now = get_time(true);
    uint8_t buffer[MAX_PACKET_SIZE]; 

    /* if packet was found, write it into packet */
    if (packet_found && ((now - last_poll_packet) >= poll_period)) {
        /* validate packet and write into buffer */
        if ((err = uartp_validate_packet(buffer, found)))
            return err;
        
        /* packet might not be found, but shouldn't error */
        if (!found)
            return OK;
        /* format packet data into struct  */
        if ((err = uartp_fill_packet(p, buffer)))
            return err;

        /* unsure about this, but this is a critical section. */
        usart_disable_rx_interrupt(USART2);
        packet_found = false;
        usart_enable_rx_interrupt(USART2);
        return OK;
    }
    *found = false;
    return OK;
}

static uart_error_t uartp_fill_packet(struct packet *p, uint8_t buffer[]) {
    if (p == NULL || buffer == NULL)
        return PACKET_NULL_POINTER;
    if ((*p).u == NULL)
        return PACKET_NULL_DATA_POINTER;
    uart_error_t err;
    uint8_t flag, len, size;
    
    /* get info from packet */
    flag = buffer[1], len = buffer[2];
    size = get_size_from_flag(flag);
    (*p).size = size;
    switch (size) {
        case 1:
            err = uartp_fill_8(p, buffer, len);
            break;
        case 2:
            err = NOT_IMPLEMENTED;
            break;
        case 4:
            err = NOT_IMPLEMENTED;
            break;
        case 8:
            err = NOT_IMPLEMENTED;
            break; 
        default:
            return PACKET_WORD_SIZE_INVALID;
    }
    return (err) ? err: OK;
}
/* @TODO fill other word sizes, bearing in mind that byte order is little-endian. */

static uart_error_t uartp_fill_8(struct packet *p, uint8_t buffer[], size_t len) {
    if (p == NULL || buffer == NULL)
        return PACKET_NULL_POINTER;
    if ((*p).u == NULL)
        return PACKET_NULL_DATA_POINTER;
    /* cast void to byte pointer */
    uint8_t *ptr = (*p).u;
    size_t i, offset;
    
    offset = 3U; // start + flag + len

    /* copy data into buffer */
    for (i = 0; i < len; i++) {
        *(ptr + i) = buffer[i + offset];
    }
    /* null terminate */
    *(ptr + i) = '\0';

    /* set length */
    (*p).len = len + 1;
    return OK;
}

static uint8_t get_size_from_flag(uint8_t flag) {
    if (1 & (flag >> fdouble))
        return 8U;
    flag &= ~(3u << fsigned);
    return flag;
}

/* @TODO errors overwrite buffer errors */
static uart_error_t uartp_validate_packet(uint8_t buffer[], bool *found) {
    if (buffer == NULL)
        return PACKET_NULL_POINTER;
    uart_error_t err;
    uint8_t flag, len, size;
    size_t i, n;
    
    i = n = 0;
    *found = false;
    
    /* if buffer empty */
    if ((err = dring_buf_empty(&_rb)))
        return err;

    /* read buffer into local buffer */
    while (!dring_buf_empty(&_rb))
        if ((err = dring_buf_read(&_rb, buffer + i++)))
            return err;
    
    /* if len buffer invalid */
    /* i is incremented either way, so it will point to the next slot, i.e. one past the end */
    n = i;
    if (!(n >= MIN_PACKET_SIZE && n <= MAX_PACKET_SIZE))
        return (err = PACKET_LENGTH_INVALID);

    /* if start and stop bytes aren't present */
    if (!(buffer[0] == PACKET_START && buffer[n-1] == PACKET_STOP))
        return (err = PACKET_MISSING_START_STOP);

    /* get info from packet */
    flag = buffer[1], len = buffer[2];
    size = get_size_from_flag(flag);

    /* if length of buffer doesn't match what flag and length says */
    if (n != (size_t)((size * len) + 4))
        return (err = PACKET_SIZE_MISMATCH);
    
    /* if none of the above occurs, packet is found */
    *found = true;
    return OK;
}


/************************* interrupts *********************/
void usart2_isr(void) {
    uart_error_t err;
    if (usart_get_flag(USART2, USART_FLAG_RXNE) == 1) {
        /* read from buffer, clearing the flag also. */
        uint8_t data = (uint8_t) usart_recv(USART2);

        /* mark the start of a packet */
        if (data == PACKET_START)
            packet_started = true;
        
        /* if we have started writing a packet, continue to. */
        if (!packet_started) 
            return;
        
        /* write into buffer */
        if ((err = dring_buf_write(&_rb, data)))
            error_handle(err);
        
        /* mark a packet as being found, for the main loop to check. */
        if (data == PACKET_STOP)
            packet_found = true;
    }
}


/***************************** util ***********************/
static void uartp_enable(void) {
    // USART2_CR1 |= USART_CR1_TE; 
}

static void uartp_disable(void) {
    // USART2_CR1 &= ~USART_CR1_TE; // disable?
}

/********************* init functions ********************/
/* adapted from https://github.com/lowbyteproductions/bare-metal-series */
uart_error_t uartp_setup(void) {
    /* enable clock to the peripheral */
    rcc_periph_clock_enable(RCC_USART2);
    rcc_periph_clock_enable(RCC_GPIOA);

    /* configure gpio pins*/
    gpio_set_af(UART_PORT, UART_AF_MODE, UART_TX_PIN | UART_RX_PIN);
    gpio_mode_setup(UART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, UART_TX_PIN | UART_RX_PIN);

    /* disble first */
    usart_disable(UART);

    /* config */
    usart_set_mode(UART, USART_MODE_TX_RX);   // duplex
    usart_set_flow_control(UART, USART_FLOWCONTROL_NONE);
    usart_set_baudrate(UART, UART_BAUD_RATE);
    usart_set_databits(UART, 8);  // byte
    usart_set_stopbits(UART, 1);
    usart_set_parity(UART, USART_PARITY_NONE);

    /* enable rx interrupt on the USART and in NVIC */
    usart_enable_rx_interrupt(UART);
    nvic_enable_irq(NVIC_USART2_IRQ);

    /* clear flag by reading from DR in case */
    usart_recv(USART2);

    /* finally enable*/
    usart_enable(UART);

    // set up buffer
    uart_error_t err = dring_buf_setup(&_rb, _buffer, MIN_PACKET_BUFFER_SIZE);
    return err;
}

extern uart_error_t uartp_teardown(void) {
    nvic_disable_irq(NVIC_USART1_IRQ);
    usart_disable_rx_interrupt(UART);
    rcc_periph_clock_disable(RCC_USART2);
    return OK;
}
/******************* endblock **********************/