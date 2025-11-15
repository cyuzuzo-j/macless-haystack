
#include "esp_gap_ble_api.h"

void test_capture_adv_data(const uint8_t *data, uint32_t len);

/**
 * Test shim that wraps esp_ble_gap_config_adv_data_raw().
 *
 * Build/link this file *instead of* redefining the function in main.c.
 * OpenHaystack still calls esp_ble_gap_config_adv_data_raw(), but this
 * wrapper captures the data for assertions.
 */
esp_err_t __wrap_esp_ble_gap_config_adv_data_raw(uint8_t *data, uint32_t len)
{
    // Call capture helper for the test
    test_capture_adv_data(data, len);

    // Optionally call the real implementation if you want BT stack behavior.
    // If you don't want to touch the controller at all, just return ESP_OK.
    return ESP_OK;
}
