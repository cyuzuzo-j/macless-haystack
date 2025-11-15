#include "esp_sleep.h"

// Implemented in test/main.c
void test_capture_sleep_enable_timer_wakeup(uint64_t time_in_us);
void test_capture_deep_sleep_start(void);

/**
 * Shim for esp_sleep_enable_timer_wakeup.
 * Captures the timeout and pretends to succeed.
 */
esp_err_t __wrap_esp_sleep_enable_timer_wakeup(uint64_t time_in_us)
{
    test_capture_sleep_enable_timer_wakeup(time_in_us);
    return ESP_OK;
}

/**
 * Shim for esp_deep_sleep_start.
 * Captures that deep sleep was requested and does not actually sleep.
 */
void __wrap_esp_deep_sleep_start(void)
{
    test_capture_deep_sleep_start();
}
