#ifndef COMMON_DEFINES_H
#define COMMON_DEFINES_H

#include <sys/types.h>
#include <stddef.h>
#include "error.h"

#define TEST_PORT (GPIOC)

#define TEST_PIN1 (GPIO8)
#define TEST_PIN2 (GPIO7)
#define TEST_PIN3 (GPIO6)
#define TEST_PIN4 (GPIO5)

extern void delay_ms(uint32_t ms);

#endif // COMMON_DEFINES_H