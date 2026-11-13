// main.c
// Step 0 bring-up: EMC pin clamp, 170 MHz PLL lock & SysTick verification
// Author: kaktys

#include "stm32g4xx.h"
#include "bsp_clock.h"
#include "bsp_gpio.h"

int main(void) {
    // Clamp gate driver pins (PA8, PA10, PA11, PB0) to GND immediately on boot
    // before waiting for PLL lock so nothing floats in Hi-Z
    bsp_gpio_emc_init();

    // Ramp core up to 170 MHz (HSI16 -> PLL, Boost Mode Range 1, 4 WS)
    bsp_clk_status_t clk_status = bsp_clock_init_170mhz();
    if (clk_status != BSP_CLK_OK) {
        // Clock init failed: fast panic blink (~10 Hz on raw 16 MHz HSI)
        while (1) {
            bsp_led_pc13_toggle();
            for (volatile uint32_t i = 0; i < 160000UL; i++) {
                __NOP();
            }
        }
    }

    // Start 1 ms system tick (NVIC prio 6)
    bsp_systick_init();

    // Quick 50 ms strobe on boot to verify cycle-accurate delay_us()
    bsp_led_pc13_on();
    delay_us(50000);
    bsp_led_pc13_off();

    uint64_t last_toggle_ms = systick_millis();

    // Step 0 heartbeat: 1 Hz square wave (500 ms half-period) on PC13
    while (1) {
        uint64_t now_ms = systick_millis();
        if ((now_ms - last_toggle_ms) >= 500ULL) {
            last_toggle_ms += 500ULL;
            bsp_led_pc13_toggle();
        }
    }
}
