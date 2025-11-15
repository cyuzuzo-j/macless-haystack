/**
 * @file mock_esp_gap_ble_api.h
 * @brief Mock header for ESP-IDF BLE GAP API functions
 * 
 * This file provides mock declarations for BLE GAP API functions
 * used in testing. It allows us to verify that openhaystack_run
 * calls the correct BLE functions with the expected data.
 */

#ifndef MOCK_ESP_GAP_BLE_API_H
#define MOCK_ESP_GAP_BLE_API_H

#include <stdint.h>
#include <stddef.h>
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mock for esp_ble_gap_config_adv_data_raw
 * 
 * This function captures the advertising data passed to it,
 * allowing tests to verify the data content.
 */
esp_err_t esp_ble_gap_config_adv_data_raw(uint8_t *raw_data, uint32_t raw_data_len);

/**
 * @brief Mock for esp_ble_gap_register_callback
 */
esp_err_t esp_ble_gap_register_callback(esp_gap_ble_cb_t callback);

/**
 * @brief Mock for esp_ble_gap_set_rand_addr
 */
esp_err_t esp_ble_gap_set_rand_addr(esp_bd_addr_t rand_addr);

/**
 * @brief Mock for esp_ble_gap_start_advertising
 */
esp_err_t esp_ble_gap_start_advertising(esp_ble_adv_params_t *adv_params);

/**
 * @brief Mock for esp_ble_gap_stop_advertising
 */
esp_err_t esp_ble_gap_stop_advertising(void);

// Helper functions for test verification

/**
 * @brief Get the last advertising data that was configured
 * 
 * @param data Output buffer for the captured data
 * @param len Output for the data length
 * @return ESP_OK if data was captured, ESP_FAIL otherwise
 */
esp_err_t mock_esp_ble_gap_get_last_adv_data(uint8_t *data, uint32_t *len);

/**
 * @brief Get the call count for esp_ble_gap_config_adv_data_raw
 */
int mock_esp_ble_gap_get_config_adv_data_call_count(void);

/**
 * @brief Reset all mock state
 */
void mock_esp_ble_gap_reset(void);

/**
 * @brief Get advertising data at a specific call index
 * 
 * @param index The call index (0-based)
 * @param data Output buffer for the captured data
 * @param len Output for the data length
 * @return ESP_OK if data was captured, ESP_FAIL otherwise
 */
esp_err_t mock_esp_ble_gap_get_adv_data_at_index(int index, uint8_t *data, uint32_t *len);

#ifdef __cplusplus
}
#endif

#endif // MOCK_ESP_GAP_BLE_API_H
