// bsp_clock.h
// 170 MHz PLL clock setup & SysTick timing for STM32G474
// Author: kaktys

#ifndef BSP_CLOCK_H
#define BSP_CLOCK_H

#include "stm32g4xx.h"
#include <stdint.h>
#include <stdbool.h>

#define BSP_SYSCLK_HZ               170000000UL
#define BSP_SYSCLK_MHZ              170UL
#define BSP_SYSTICK_FREQ_HZ         1000UL

// NVIC priorities (0 = highest, 15 = lowest)
#define BSP_NVIC_PRIO_HRTIM_FLT     0UL  // hardware OVP fault - kills PWM instantly
#define BSP_NVIC_PRIO_COMP_HV       1UL  // HV_BUS comparator - stops boost at target V
#define BSP_NVIC_PRIO_HRTIM_MST     2UL  // ZVS zero-crossing sync turn-off
#define BSP_NVIC_PRIO_EXTI_FIRE     4UL  // fire trigger (PB3)
#define BSP_NVIC_PRIO_EXTI_ENC      5UL  // encoder button (PB2)
#define BSP_NVIC_PRIO_SYSTICK       6UL  // 1ms tick & telemetry

// Spin-loop limits so we don't hang forever if a hardware flag gets stuck
#define BSP_CLK_TIMEOUT_HSI         50000UL
#define BSP_CLK_TIMEOUT_PWR         50000UL
#define BSP_CLK_TIMEOUT_FLASH       50000UL
#define BSP_CLK_TIMEOUT_PLL         100000UL
#define BSP_CLK_TIMEOUT_SWS         100000UL

typedef enum {
    BSP_CLK_OK = 0,
    BSP_CLK_ERR_PWR_VOS_TIMEOUT,
    BSP_CLK_ERR_FLASH_LATENCY_TIMEOUT,
    BSP_CLK_ERR_HSI_TIMEOUT,
    BSP_CLK_ERR_PLL_STOP_TIMEOUT,
    BSP_CLK_ERR_PLL_LOCK_TIMEOUT,
    BSP_CLK_ERR_SWS_SWITCH_TIMEOUT
} bsp_clk_status_t;

bsp_clk_status_t bsp_clock_init_170mhz(void);
void bsp_systick_init(void);
uint64_t systick_millis(void);
void delay_us(uint32_t us);

#endif // BSP_CLOCK_H
