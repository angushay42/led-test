#ifndef UARTP_H
#define UARTP_H

#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>

#include <sys/types.h>

#include "common-defines.h"
#include "ringbuffer.h"

// USART2 is the only one that can use ST-Link
#define UART            (USART2)
#define UART_PORT       (GPIOA)
#define UART_TX_PIN     (GPIO2)
#define UART_RX_PIN     (GPIO3)

#define UART_AF_MODE    (GPIO_AF7)
#define UART_BAUD_RATE  (115200) // from Google

/* start - flag - len - len*data - stop. so 5, len cannot be 0. */
/* min size is 1 */
/* absolute minimum size of a packet */
#define MIN_PACKET_SIZE (5)

/* len is in range [1, 255]. so 5 (min) + (max_size * 254)  */
/* absolutle maximum size of a packet */
#define MAX_PACKET_SIZE (MIN_PACKET_SIZE - 1 + (8 * 254))

/* minimum size a ringbuffer needs to have to fit a packet. */
#define MIN_PACKET_BUFFER_SIZE (1 << 11)

#define PACKET_START    ((uint8_t)'{')
#define PACKET_STOP     ((uint8_t)'}')

/* flag for superloop to read the packet in the buffer */
extern volatile bool packet_found;

// adapted from https://stackoverflow.com/a/44611722
struct packet {
    /* string identifier */
    char *id;
    /* union means each member occupies the same memory space, so it can be accessed depending on how you use it */
    union {
        double *f; 
        void *u;
    };
    size_t size;
    size_t len;
    bool is_signed;
};


extern error_t uartp_setup(void);
extern error_t uartp_teardown(void);

extern error_t uartp_send_packet(struct packet *p);
extern error_t uartp_poll_packet(struct packet *p, uint64_t poll_period, bool *found);



#endif  // UARTP_H