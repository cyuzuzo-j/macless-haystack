#include "esp_gap_ble_api.h"

// Capture state for tests
static esp_bd_addr_t last_rand_addr = {0};
static int set_rand_addr_call_count = 0;

int test_get_set_rand_addr_call_count(void)
{
    return set_rand_addr_call_count;
}

void test_get_last_rand_addr(esp_bd_addr_t out_addr)
{
    if (!out_addr) {
        return;
    }
    for (int i = 0; i < ESP_BD_ADDR_LEN; ++i) {
        out_addr[i] = last_rand_addr[i];
    }
}

/**
 * Shim for esp_ble_gap_set_rand_addr().
 *
 * Captures the address and pretends the call succeeded.
 */
esp_err_t __wrap_esp_ble_gap_set_rand_addr(esp_bd_addr_t rand_addr)
{
    for (int i = 0; i < ESP_BD_ADDR_LEN; ++i) {
        last_rand_addr[i] = rand_addr[i];
    }
    set_rand_addr_call_count++;
    return ESP_OK;
}
