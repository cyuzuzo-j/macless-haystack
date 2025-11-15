/**
 * @file mock_esp_gap_ble_api.c
 * @brief Mock implementation for ESP-IDF BLE GAP API functions
 */

#include "mock_esp_gap_ble_api.h"
#include <string.h>
#include "esp_err.h"

// Storage for captured advertising data
#define MAX_ADV_DATA_LEN 31
static uint8_t captured_adv_data[10][MAX_ADV_DATA_LEN];  // Store up to 10 calls
static uint32_t captured_adv_data_len[10];
static int adv_data_call_count = 0;

// Storage for other function calls
static esp_gap_ble_cb_t registered_callback = NULL;
static esp_bd_addr_t captured_rand_addr;
static int register_callback_count = 0;
static int set_rand_addr_count = 0;
static int start_advertising_count = 0;
static int stop_advertising_count = 0;

esp_err_t esp_ble_gap_config_adv_data_raw(uint8_t *raw_data, uint32_t raw_data_len)
{
    if (raw_data == NULL || raw_data_len > MAX_ADV_DATA_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    if (adv_data_call_count < 10) {
        memcpy(captured_adv_data[adv_data_call_count], raw_data, raw_data_len);
        captured_adv_data_len[adv_data_call_count] = raw_data_len;
        adv_data_call_count++;
    }

    return ESP_OK;
}

esp_err_t esp_ble_gap_register_callback(esp_gap_ble_cb_t callback)
{
    registered_callback = callback;
    register_callback_count++;
    return ESP_OK;
}

esp_err_t esp_ble_gap_set_rand_addr(esp_bd_addr_t rand_addr)
{
    memcpy(captured_rand_addr, rand_addr, sizeof(esp_bd_addr_t));
    set_rand_addr_count++;
    return ESP_OK;
}

esp_err_t esp_ble_gap_start_advertising(esp_ble_adv_params_t *adv_params)
{
    start_advertising_count++;
    return ESP_OK;
}

esp_err_t esp_ble_gap_stop_advertising(void)
{
    stop_advertising_count++;
    return ESP_OK;
}

esp_err_t mock_esp_ble_gap_get_last_adv_data(uint8_t *data, uint32_t *len)
{
    if (adv_data_call_count == 0) {
        return ESP_FAIL;
    }

    int last_index = adv_data_call_count - 1;
    memcpy(data, captured_adv_data[last_index], captured_adv_data_len[last_index]);
    *len = captured_adv_data_len[last_index];
    return ESP_OK;
}

int mock_esp_ble_gap_get_config_adv_data_call_count(void)
{
    return adv_data_call_count;
}

void mock_esp_ble_gap_reset(void)
{
    adv_data_call_count = 0;
    register_callback_count = 0;
    set_rand_addr_count = 0;
    start_advertising_count = 0;
    stop_advertising_count = 0;
    registered_callback = NULL;
    memset(captured_adv_data, 0, sizeof(captured_adv_data));
    memset(captured_adv_data_len, 0, sizeof(captured_adv_data_len));
    memset(captured_rand_addr, 0, sizeof(captured_rand_addr));
}

// Get specific call's advertising data
esp_err_t mock_esp_ble_gap_get_adv_data_at_index(int index, uint8_t *data, uint32_t *len)
{
    if (index < 0 || index >= adv_data_call_count) {
        return ESP_FAIL;
    }

    memcpy(data, captured_adv_data[index], captured_adv_data_len[index]);
    *len = captured_adv_data_len[index];
    return ESP_OK;
}
