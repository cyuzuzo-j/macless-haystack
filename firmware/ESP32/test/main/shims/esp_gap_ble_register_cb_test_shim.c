#include "esp_gap_ble_api.h"

// Optional: expose last registered callback for assertions
static esp_gap_ble_cb_t last_registered_cb = NULL;
static int register_call_count = 0;

esp_gap_ble_cb_t test_get_last_registered_gap_cb(void)
{
    return last_registered_cb;
}

int test_get_gap_register_call_count(void)
{
    return register_call_count;
}

/**
 * Shim for esp_ble_gap_register_callback().
 *
 * Captures the callback pointer and pretends that registration succeeded.
 * Prevents the test binary from touching the real BT stack.
 */
esp_err_t __wrap_esp_ble_gap_register_callback(esp_gap_ble_cb_t callback)
{
    last_registered_cb = callback;
    register_call_count++;
    return ESP_OK;
}
