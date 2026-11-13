// bsp_gpio.h
// GPIO init, EMC pin lockdown & onboard PC13 LED helpers
// Author: kaktys

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "stm32g4xx.h"
#include <stdint.h>
#include <stdbool.h>

// Output SYSCLK/16 (10.625 MHz) on PA8 (MCO) to verify PLL clock on scope.
// WARNING: PA8 is HRTIM1_CHA1 (SiC gate driver input)!
// Set to 1 ONLY on a bare WeAct board. Keep strictly 0 on the power PCB!
#ifndef BSP_STEP0_ENABLE_MCO_PA8
#define BSP_STEP0_ENABLE_MCO_PA8    0
#endif

void bsp_gpio_emc_init(void);
void bsp_gpio_mco_pa8_enable(void);

// WeAct G474 onboard LED is on PC13 (active high)
static inline void bsp_led_pc13_on(void) {
    GPIOC->BSRR = GPIO_BSRR_BS13;
}

static inline void bsp_led_pc13_off(void) {
    GPIOC->BSRR = GPIO_BSRR_BR13;
}

static inline void bsp_led_pc13_toggle(void) {
    if (GPIOC->ODR & GPIO_ODR_OD13) {
        GPIOC->BSRR = GPIO_BSRR_BR13;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BS13;
    }
}

#endif // BSP_GPIO_H
