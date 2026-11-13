// bsp_clock.c
// Bare-metal 170 MHz clock bring-up & lockless SysTick timer
// Author: kaktys

#include "bsp_clock.h"

static volatile uint32_t g_systick_ms_low  = 0;
static volatile uint32_t g_systick_ms_high = 0;

static inline void bsp_fpu_enable(void) {
    // Enable full access to CP10 & CP11 (hardware float)
    SCB->CPACR |= (3UL << (10UL * 2UL)) | (3UL << (11UL * 2UL));
    __DSB();
    __ISB();
}

bsp_clk_status_t bsp_clock_init_170mhz(void) {
    uint32_t timeout;

    bsp_fpu_enable();
    NVIC_SetPriorityGrouping(3UL); // 4 bits preemption (0..15), 0 subpriority

    // Power controller clock on
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    __DSB();

    // Set core voltage scaling to Range 1 (VOS = 01b)
    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS_Msk) | (1UL << PWR_CR1_VOS_Pos);
    __DSB();

    timeout = BSP_CLK_TIMEOUT_PWR;
    while (PWR->SR2 & PWR_SR2_VOSF) {
        if (--timeout == 0) return BSP_CLK_ERR_PWR_VOS_TIMEOUT;
    }

    // Enable Range 1 Boost Mode (R1MODE = 0) - required for >150 MHz on G4
    PWR->CR5 &= ~PWR_CR5_R1MODE;
    __DSB();

    // Flash setup for 170 MHz: 4 Wait States + I/D cache + prefetch
    FLASH->ACR = FLASH_ACR_LATENCY_4WS | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;
    __DSB();
    __ISB();

    timeout = BSP_CLK_TIMEOUT_FLASH;
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != FLASH_ACR_LATENCY_4WS) {
        if (--timeout == 0) return BSP_CLK_ERR_FLASH_LATENCY_TIMEOUT;
    }

    // Turn on internal 16 MHz RC (HSI16)
    RCC->CR |= RCC_CR_HSION;
    __DSB();

    timeout = BSP_CLK_TIMEOUT_HSI;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {
        if (--timeout == 0) return BSP_CLK_ERR_HSI_TIMEOUT;
    }

    // Disable PLL before touching multipliers
    RCC->CR &= ~RCC_CR_PLLON;
    __DSB();

    timeout = BSP_CLK_TIMEOUT_PLL;
    while (RCC->CR & RCC_CR_PLLRDY) {
        if (--timeout == 0) return BSP_CLK_ERR_PLL_STOP_TIMEOUT;
    }

    // PLL math: 16 MHz (HSI) / 4 (PLLM) * 85 (PLLN) / 2 (PLLR) = 170 MHz
    // VCO runs at 340 MHz
    RCC->PLLCFGR = (2UL << RCC_PLLCFGR_PLLSRC_Pos) | // src = HSI16
                   (3UL << RCC_PLLCFGR_PLLM_Pos)   | // /4  (0011b)
                   (85UL << RCC_PLLCFGR_PLLN_Pos)  | // *85
                   (0UL << RCC_PLLCFGR_PLLR_Pos)   | // /2  (00b)
                   RCC_PLLCFGR_PLLREN;               // enable main R output
    __DSB();

    // Fire up PLL and wait for lock
    RCC->CR |= RCC_CR_PLLON;
    __DSB();

    timeout = BSP_CLK_TIMEOUT_PLL;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {
        if (--timeout == 0) return BSP_CLK_ERR_PLL_LOCK_TIMEOUT;
    }

    // RM0440 step-up rule: step AHB through /2 (85 MHz) before jumping to 170 MHz
    // so the internal LDO doesn't brown out from current spike
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE_Msk | RCC_CFGR_PPRE1_Msk | RCC_CFGR_PPRE2_Msk)) |
                RCC_CFGR_HPRE_DIV2  |
                RCC_CFGR_PPRE1_DIV1 |
                RCC_CFGR_PPRE2_DIV1;
    __DSB();

    // Switch system clock mux to PLL
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_PLL;
    __DSB();
    __ISB();

    timeout = BSP_CLK_TIMEOUT_SWS;
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL) {
        if (--timeout == 0) return BSP_CLK_ERR_SWS_SWITCH_TIMEOUT;
    }

    // Wait >1 us at 85 MHz for core regulator to settle
    for (volatile uint32_t i = 0; i < 200; i++) {
        __NOP();
    }

    // Full throttle: AHB divider = /1 (HCLK = 170 MHz)
    RCC->CFGR &= ~RCC_CFGR_HPRE_Msk;
    __DSB();
    __ISB();

    return BSP_CLK_OK;
}

void bsp_systick_init(void) {
    SysTick->CTRL = 0;
    SysTick->LOAD = (BSP_SYSCLK_HZ / BSP_SYSTICK_FREQ_HZ) - 1UL; // 169999 ticks = 1 ms
    SysTick->VAL  = 0;

    NVIC_SetPriority(SysTick_IRQn, BSP_NVIC_PRIO_SYSTICK);

    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
}

// Lock-free 64-bit read so we never disable IRQs and mess up HRTIM/COMP latency
uint64_t systick_millis(void) {
    uint32_t hi1, lo, hi2;
    uint32_t guard = 10;

    do {
        hi1 = g_systick_ms_high;
        lo  = g_systick_ms_low;
        hi2 = g_systick_ms_high;
    } while ((hi1 != hi2) && (--guard > 0));

    return ((uint64_t)hi2 << 32) | lo;
}

// Cycle-accurate us delay polling SysTick->VAL directly
void delay_us(uint32_t us) {
    if (us == 0) return;

    const uint32_t reload = SysTick->LOAD + 1UL;
    uint64_t total_ticks  = (uint64_t)us * BSP_SYSCLK_MHZ;
    uint64_t elapsed      = 0;
    uint32_t last_val     = SysTick->VAL;
    uint32_t max_loops    = (us * 256UL) + 1000UL;

    while ((elapsed < total_ticks) && (--max_loops > 0)) {
        uint32_t now_val = SysTick->VAL;
        if (now_val != last_val) {
            if (now_val < last_val) {
                elapsed += (last_val - now_val);
            } else {
                elapsed += (reload - now_val + last_val); // wrapped around 0
            }
            last_val = now_val;
        }
    }
}

void SysTick_Handler(void) {
    uint32_t next_low = g_systick_ms_low + 1UL;
    g_systick_ms_low  = next_low;
    if (next_low == 0) {
        g_systick_ms_high++;
    }
}
