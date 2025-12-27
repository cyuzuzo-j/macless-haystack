/**
 * @file main.c
 * @brief Unit tests to check if the firmware advertises rolling codes
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "unity.h"
#include "unity_fixture.h"

#include "esp_log.h"
#include "openhaystack.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"



static uint8_t __attribute__((unused)) test_public_key[28] = {
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, // First 6 bytes
    0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44, // Bytes 6-11
    0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, // Bytes 12-17
    0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, // Bytes 18-23
    0x11, 0x22, 0x33, 0x44              // Bytes 24-27
};

// ---------------------------------------------------------------------------
// Test-side capture helpers (called from a shim, not from esp-idf itself)
// ---------------------------------------------------------------------------

static uint8_t last_adv_data[31];
static int last_adv_data_len = 0;
static int adv_data_call_count = 0;

static uint64_t last_sleep_timeout_us = 0;
static int sleep_enable_call_count = 0;
static int deep_sleep_start_call_count = 0;

void test_capture_adv_data(const uint8_t *data, uint32_t len)
{
    if (len > sizeof(last_adv_data)) {
        len = sizeof(last_adv_data);
    }
    memcpy(last_adv_data, data, len);
    last_adv_data_len = (int)len;
    adv_data_call_count++;
}

void test_capture_sleep_enable_timer_wakeup(uint64_t time_in_us)
{
    last_sleep_timeout_us = time_in_us;
    sleep_enable_call_count++;
}

void test_capture_deep_sleep_start(void)
{
    deep_sleep_start_call_count++;
}

static void reset_sleep_captures(void)
{
    last_sleep_timeout_us = 0;
    sleep_enable_call_count = 0;
    deep_sleep_start_call_count = 0;
}

// ---------------------------------------------------------------------------
// Unity group + tests
// ---------------------------------------------------------------------------

TEST_GROUP(openhaystack_integration);
TEST_SETUP(openhaystack_integration)
{
    reset_sleep_captures();
}
TEST_TEAR_DOWN(openhaystack_integration)
{
}

/**
 * @brief Test that multiple openhaystack_run calls use the same advertisement data
 * 
 * This proves non-rolling behavior by showing that
 * esp_ble_gap_config_adv_data_raw is called with identical data across multiple runs. 
 */
TEST(openhaystack_integration, multiple_runs_same_adv_data_within_cycle)
{
    TEST_ASSERT_EQUAL(0, openhaystack_init());

    uint8_t first_call_adv_data[31];

    const uint8_t TEST_REUSE_CYCLES = 5;
    const uint8_t TEST_DELAY_S = 1;

    for (int i = 0; i <= TEST_REUSE_CYCLES + 1; i++) {
        openhaystack_run(TEST_DELAY_S, TEST_REUSE_CYCLES);

        // On first iteration, store data
        if (i == 0) {
            memcpy(first_call_adv_data, last_adv_data, last_adv_data_len);

        } else if (i <= TEST_REUSE_CYCLES) {
            // Runs 1..TEST_REUSE_CYCLES should match Run 0
            TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(
                first_call_adv_data,
                last_adv_data,
                last_adv_data_len,
                "Advertisement data should match during reuse window"
            );
        } else {
            // i == TEST_REUSE_CYCLES + 1 -> Should have rolled.
            TEST_ASSERT_NOT_EQUAL(0, memcmp(first_call_adv_data, last_adv_data, last_adv_data_len));
        }
    }
}
TEST_GROUP_RUNNER(openhaystack_integration)
{
    RUN_TEST_CASE(openhaystack_integration, multiple_runs_same_adv_data_within_cycle);
}

static void run_all_tests(void)
{
    printf("Macless Haystack Test - Non-Rolling Code Privacy Test Suite\n");
    printf("=============================================================\n\n");
    RUN_TEST_GROUP(openhaystack_integration);
}

// Test application main
void app_main(void)
{
    UNITY_MAIN_FUNC(run_all_tests);
}
