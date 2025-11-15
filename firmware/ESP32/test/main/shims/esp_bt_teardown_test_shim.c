#include "esp_bt.h"
#include "esp_bt_main.h"

// Simple counters so tests can assert teardown is requested
static int bluedroid_disable_call_count = 0;
static int bluedroid_deinit_call_count = 0;
static int bt_controller_disable_call_count = 0;
static int bt_controller_deinit_call_count = 0;

int test_get_bluedroid_disable_call_count(void)
{
    return bluedroid_disable_call_count;
}

int test_get_bluedroid_deinit_call_count(void)
{
    return bluedroid_deinit_call_count;
}

int test_get_bt_controller_disable_call_count(void)
{
    return bt_controller_disable_call_count;
}

int test_get_bt_controller_deinit_call_count(void)
{
    return bt_controller_deinit_call_count;
}

// Shim for esp_bluedroid_disable()
esp_err_t __wrap_esp_bluedroid_disable(void)
{
    bluedroid_disable_call_count++;
    return ESP_OK;
}

// Shim for esp_bluedroid_deinit()
esp_err_t __wrap_esp_bluedroid_deinit(void)
{
    bluedroid_deinit_call_count++;
    return ESP_OK;
}

// Shim for esp_bt_controller_disable()
esp_err_t __wrap_esp_bt_controller_disable(void)
{
    bt_controller_disable_call_count++;
    return ESP_OK;
}

// Shim for esp_bt_controller_deinit()
esp_err_t __wrap_esp_bt_controller_deinit(void)
{
    bt_controller_deinit_call_count++;
    return ESP_OK;
}
