// bsp_gpio.c
// Safe startup pin states & EMC analog lockdown
// Author: kaktys

#include "bsp_gpio.h"

void bsp_gpio_emc_init(void) {
    // Enable GPIOA, GPIOB, GPIOC clocks on AHB2
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN;
    __DSB();

    // --- GPIOA ---
    // PA8 (SiC IN+), PA10 (ZVS INB), PA11 (ZVS INA): clamp to Output Push-Pull LOW (0V)
    // before HRTIM starts so UCC27511 / UCC27524 inputs never float in Hi-Z.
    // PA13 (SWDIO) & PA14 (SWCLK): keep in AF0 with pull-up/pull-down so we don't brick SWD.
    // Everything else: Analog mode (11b) to disconnect Schmitt triggers from HV noise.
    GPIOA->BSRR = GPIO_BSRR_BR8 | GPIO_BSRR_BR10 | GPIO_BSRR_BR11;
    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT8 | GPIO_OTYPER_OT10 | GPIO_OTYPER_OT11);
    GPIOA->OSPEEDR &= ~((3UL << (8UL * 2UL)) | (3UL << (10UL * 2UL)) | (3UL << (11UL * 2UL)));

    // AF0 for PA13 & PA14
    GPIOA->AFR[1] &= ~((0xFUL << ((13UL - 8UL) * 4UL)) | (0xFUL << ((14UL - 8UL) * 4UL)));

    // Pull-up (01b) on PA13, Pull-down (10b) on PA14, no pulls on others
    GPIOA->PUPDR = (1UL << (13UL * 2UL)) | (2UL << (14UL * 2UL));

    // MODER: default all to Analog (11b), override PA8/10/11 to Output (01b) and PA13/14 to AF (10b)
    GPIOA->MODER = (0xFFFFFFFFUL & ~(
                        (3UL << (8UL * 2UL))  |
                        (3UL << (10UL * 2UL)) |
                        (3UL << (11UL * 2UL)) |
                        (3UL << (13UL * 2UL)) |
                        (3UL << (14UL * 2UL))
                   )) |
                   (1UL << (8UL * 2UL))  |
                   (1UL << (10UL * 2UL)) |
                   (1UL << (11UL * 2UL)) |
                   (2UL << (13UL * 2UL)) |
                   (2UL << (14UL * 2UL));

    // --- GPIOB ---
    // PB0 (AO3400A gas solenoid gate): force Output LOW (0V) to back up external 10k R46.
    // PB1..PB15 (including old JTAG pins PB3/PB4): Analog mode (11b).
    GPIOB->BSRR    = GPIO_BSRR_BR0;
    GPIOB->OTYPER  &= ~GPIO_OTYPER_OT0;
    GPIOB->OSPEEDR &= ~(3UL << (0UL * 2UL));
    GPIOB->PUPDR   = 0x00000000UL;
    GPIOB->MODER   = (0xFFFFFFFFUL & ~(3UL << (0UL * 2UL))) | (1UL << (0UL * 2UL));

    // --- GPIOC ---
    // PC13 (WeAct onboard LED): Output Push-Pull Low Speed.
    // All other pins: Analog mode (11b).
    GPIOC->BSRR    = GPIO_BSRR_BR13;
    GPIOC->OTYPER  &= ~GPIO_OTYPER_OT13;
    GPIOC->OSPEEDR &= ~(3UL << (13UL * 2UL));
    GPIOC->PUPDR   = 0x00000000UL;
    GPIOC->MODER   = (0xFFFFFFFFUL & ~(3UL << (13UL * 2UL))) | (1UL << (13UL * 2UL));

#if (BSP_STEP0_ENABLE_MCO_PA8 == 1)
    bsp_gpio_mco_pa8_enable();
#endif

    __DSB();
    __ISB();
}

void bsp_gpio_mco_pa8_enable(void) {
    // Route SYSCLK (0001b) with /16 prescaler (100b) to MCO -> 10.625 MHz
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCOSEL_Msk | RCC_CFGR_MCOPRE_Msk)) |
                (4UL << RCC_CFGR_MCOPRE_Pos) |
                (1UL << RCC_CFGR_MCOSEL_Pos);
    __DSB();

    // Set PA8 to AF0 (MCO), Very High Speed (11b), Push-Pull
    GPIOA->OTYPER  &= ~GPIO_OTYPER_OT8;
    GPIOA->OSPEEDR |= (3UL << (8UL * 2UL));
    GPIOA->PUPDR   &= ~(3UL << (8UL * 2UL));
    GPIOA->AFR[1]  &= ~(0xFUL << ((8UL - 8UL) * 4UL));
    GPIOA->MODER   = (GPIOA->MODER & ~(3UL << (8UL * 2UL))) | (2UL << (8UL * 2UL));
    __DSB();
}
