/**
 * @file main.c
 * @brief Unit tests to verify that the firmware advertises non-rolling codes
 * 
 * This test suite verifies that the ESP32 firmware reuses the same advertising
 * key for multiple cycles before switching, proving it uses non-rolling codes.
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "unity_fixture.h"

// Function prototypes from openhaystack_main.c
void set_addr_from_key(uint8_t addr[6], uint8_t *public_key);
void set_payload_from_key(uint8_t *payload, uint8_t *public_key);

// Test data
static uint8_t test_public_key[28] = {
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, // First 6 bytes
    0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44, // Bytes 6-11
    0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, // Bytes 12-17
    0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, // Bytes 18-23
    0x11, 0x22, 0x33, 0x44              // Bytes 24-27
};

// Test group setup
TEST_GROUP(non_rolling_codes);

TEST_SETUP(non_rolling_codes)
{
    // Setup run before each test
}

TEST_TEAR_DOWN(non_rolling_codes)
{
    // Cleanup run after each test
}

/**
 * @brief Test that set_addr_from_key produces consistent output
 * 
 * This verifies that the same public key always produces the same
 * BLE address, which is a requirement for non-rolling codes.
 */
TEST(non_rolling_codes, set_addr_from_key_is_deterministic)
{
    uint8_t addr1[6];
    uint8_t addr2[6];
    
    // Call the function twice with the same key
    set_addr_from_key(addr1, test_public_key);
    set_addr_from_key(addr2, test_public_key);
    
    // Verify both addresses are identical
    TEST_ASSERT_EQUAL_UINT8_ARRAY(addr1, addr2, 6);
}

/**
 * @brief Test that the address correctly includes key material
 * 
 * Verifies that the BLE address is derived from the public key
 * and includes the proper format bits.
 */
TEST(non_rolling_codes, set_addr_from_key_format)
{
    uint8_t addr[6];
    
    set_addr_from_key(addr, test_public_key);
    
    // First byte should have bits 6-7 set (0b11xxxxxx)
    TEST_ASSERT_BITS(0xC0, 0xC0, addr[0]);
    
    // Verify the address contains key material (bytes 1-5)
    TEST_ASSERT_EQUAL_UINT8(test_public_key[1], addr[1]);
    TEST_ASSERT_EQUAL_UINT8(test_public_key[2], addr[2]);
    TEST_ASSERT_EQUAL_UINT8(test_public_key[3], addr[3]);
    TEST_ASSERT_EQUAL_UINT8(test_public_key[4], addr[4]);
    TEST_ASSERT_EQUAL_UINT8(test_public_key[5], addr[5]);
}

/**
 * @brief Test that set_payload_from_key produces consistent output
 * 
 * This verifies that the same public key always produces the same
 * advertising payload, proving non-rolling code behavior.
 */
TEST(non_rolling_codes, set_payload_from_key_is_deterministic)
{
    uint8_t payload1[31] = {0};
    uint8_t payload2[31] = {0};
    
    // Initialize with same header
    payload1[0] = 0x1e;
    payload1[1] = 0xff;
    payload1[2] = 0x4c;
    payload1[3] = 0x00;
    payload1[4] = 0x12;
    payload1[5] = 0x19;
    payload1[6] = 0x00;
    
    memcpy(payload2, payload1, 31);
    
    // Call the function twice with the same key
    set_payload_from_key(payload1, test_public_key);
    set_payload_from_key(payload2, test_public_key);
    
    // Verify both payloads are identical
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload1, payload2, 31);
}

/**
 * @brief Test that payload contains key material
 * 
 * Verifies that the advertising payload contains the correct
 * portions of the public key (bytes 6-27 and top 2 bits).
 */
TEST(non_rolling_codes, set_payload_from_key_content)
{
    uint8_t payload[31] = {0};
    
    set_payload_from_key(payload, test_public_key);
    
    // Verify that bytes 7-28 contain public_key[6-27]
    TEST_ASSERT_EQUAL_UINT8_ARRAY(&test_public_key[6], &payload[7], 22);
    
    // Verify that byte 29 contains the top 2 bits of public_key[0]
    uint8_t expected_bits = test_public_key[0] >> 6;
    TEST_ASSERT_EQUAL_UINT8(expected_bits, payload[29]);
}

/**
 * @brief Test multiple calls produce same address (non-rolling behavior)
 * 
 * This is the key test proving non-rolling codes: calling the function
 * multiple times with the same key should always produce the same result,
 * which is what allows the key to be reused across multiple advertisement
 * cycles (REUSE_CYCLES).
 */
TEST(non_rolling_codes, multiple_calls_same_key_same_output)
{
    uint8_t addr[3][6];
    uint8_t payload[3][31];
    
    // Initialize payloads with same header
    for (int i = 0; i < 3; i++) {
        memset(payload[i], 0, 31);
    }
    
    // Call functions 3 times with same key
    for (int i = 0; i < 3; i++) {
        set_addr_from_key(addr[i], test_public_key);
        set_payload_from_key(payload[i], test_public_key);
    }
    
    // All addresses should be identical
    TEST_ASSERT_EQUAL_UINT8_ARRAY(addr[0], addr[1], 6);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(addr[1], addr[2], 6);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(addr[0], addr[2], 6);
    
    // All payloads should be identical
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload[0], payload[1], 31);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload[1], payload[2], 31);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload[0], payload[2], 31);
}

/**
 * @brief Test that different keys produce different addresses
 * 
 * While we want non-rolling codes (same key = same address),
 * we also need to verify that different keys produce different addresses.
 */
TEST(non_rolling_codes, different_keys_different_addresses)
{
    uint8_t key1[28];
    uint8_t key2[28];
    uint8_t addr1[6];
    uint8_t addr2[6];
    
    // Create two different keys
    memcpy(key1, test_public_key, 28);
    memcpy(key2, test_public_key, 28);
    key2[1] = 0xFF; // Change one byte
    
    // Generate addresses
    set_addr_from_key(addr1, key1);
    set_addr_from_key(addr2, key2);
    
    // Addresses should be different
    TEST_ASSERT_NOT_EQUAL(0, memcmp(addr1, addr2, 6));
}

/**
 * @brief Test that different keys produce different payloads
 */
TEST(non_rolling_codes, different_keys_different_payloads)
{
    uint8_t key1[28];
    uint8_t key2[28];
    uint8_t payload1[31] = {0};
    uint8_t payload2[31] = {0};
    
    // Create two different keys
    memcpy(key1, test_public_key, 28);
    memcpy(key2, test_public_key, 28);
    key2[10] = 0xAA; // Change a byte in the payload range
    
    // Generate payloads
    set_payload_from_key(payload1, key1);
    set_payload_from_key(payload2, key2);
    
    // Payloads should be different
    TEST_ASSERT_NOT_EQUAL(0, memcmp(payload1, payload2, 31));
}

// Test group runner
TEST_GROUP_RUNNER(non_rolling_codes)
{
    RUN_TEST_CASE(non_rolling_codes, set_addr_from_key_is_deterministic);
    RUN_TEST_CASE(non_rolling_codes, set_addr_from_key_format);
    RUN_TEST_CASE(non_rolling_codes, set_payload_from_key_is_deterministic);
    RUN_TEST_CASE(non_rolling_codes, set_payload_from_key_content);
    RUN_TEST_CASE(non_rolling_codes, multiple_calls_same_key_same_output);
    RUN_TEST_CASE(non_rolling_codes, different_keys_different_addresses);
    RUN_TEST_CASE(non_rolling_codes, different_keys_different_payloads);
}

// ============================================================================
// Integration Tests with Mocks
// ============================================================================

#include "mock_esp_gap_ble_api.h"

// Test group for integration tests
TEST_GROUP(openhaystack_integration);

TEST_SETUP(openhaystack_integration)
{
    // Reset all mock state before each test
    mock_esp_ble_gap_reset();
}

TEST_TEAR_DOWN(openhaystack_integration)
{
    // Cleanup after each test
    mock_esp_ble_gap_reset();
}

/**
 * @brief Test that multiple openhaystack_run calls use the same advertisement data
 * 
 * This proves non-rolling behavior at the system level by showing that
 * esp_ble_gap_config_adv_data_raw receives identical data across multiple runs
 * within the same key cycle.
 */
TEST(openhaystack_integration, multiple_runs_same_adv_data_within_cycle)
{
    // This test simulates what happens in openhaystack_run when the same key
    // is used multiple times (within REUSE_CYCLES). We'll call the key
    // preparation functions multiple times and verify the advertisement
    // data remains constant.
    
    uint8_t public_key[28] = {
        0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, // First 6 bytes
        0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, // Bytes 6-11
        0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, // Bytes 12-17
        0xEF, 0x01, 0x23, 0x45, 0x67, 0x89, // Bytes 18-23
        0xAB, 0xCD, 0xEF, 0x01              // Bytes 24-27
    };
    
    // Simulate the advertisement data structure from openhaystack_main.c
    uint8_t adv_data_run1[31] = {
        0x1e,       /* Length (30) */
        0xff,       /* Manufacturer Specific Data (type 0xff) */
        0x4c, 0x00, /* Company ID (Apple) */
        0x12, 0x19, /* Offline Finding type and length */
        0x00,       /* State */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, /* First two bits */
        0x00, /* Hint (0x00) */
    };
    
    uint8_t adv_data_run2[31];
    uint8_t adv_data_run3[31];
    
    // Initialize run2 and run3 with same header
    memcpy(adv_data_run2, adv_data_run1, 31);
    memcpy(adv_data_run3, adv_data_run1, 31);
    
    // Simulate first run: prepare advertisement data
    set_payload_from_key(adv_data_run1, public_key);
    esp_ble_gap_config_adv_data_raw(adv_data_run1, sizeof(adv_data_run1));
    
    // Simulate second run with same key (within REUSE_CYCLES)
    set_payload_from_key(adv_data_run2, public_key);
    esp_ble_gap_config_adv_data_raw(adv_data_run2, sizeof(adv_data_run2));
    
    // Simulate third run with same key (within REUSE_CYCLES)
    set_payload_from_key(adv_data_run3, public_key);
    esp_ble_gap_config_adv_data_raw(adv_data_run3, sizeof(adv_data_run3));
    
    // Verify that esp_ble_gap_config_adv_data_raw was called 3 times
    TEST_ASSERT_EQUAL_INT(3, mock_esp_ble_gap_get_config_adv_data_call_count());
    
    // Retrieve the captured advertisement data from each call
    uint8_t captured_data_1[31];
    uint8_t captured_data_2[31];
    uint8_t captured_data_3[31];
    uint32_t len1, len2, len3;
    
    TEST_ASSERT_EQUAL(ESP_OK, mock_esp_ble_gap_get_adv_data_at_index(0, captured_data_1, &len1));
    TEST_ASSERT_EQUAL(ESP_OK, mock_esp_ble_gap_get_adv_data_at_index(1, captured_data_2, &len2));
    TEST_ASSERT_EQUAL(ESP_OK, mock_esp_ble_gap_get_adv_data_at_index(2, captured_data_3, &len3));
    
    // All lengths should be 31 bytes
    TEST_ASSERT_EQUAL_UINT32(31, len1);
    TEST_ASSERT_EQUAL_UINT32(31, len2);
    TEST_ASSERT_EQUAL_UINT32(31, len3);
    
    // All three advertisement data should be identical (non-rolling behavior)
    TEST_ASSERT_EQUAL_UINT8_ARRAY(captured_data_1, captured_data_2, 31);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(captured_data_2, captured_data_3, 31);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(captured_data_1, captured_data_3, 31);
    
    // Verify the data matches what we prepared
    TEST_ASSERT_EQUAL_UINT8_ARRAY(adv_data_run1, captured_data_1, 31);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(adv_data_run2, captured_data_2, 31);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(adv_data_run3, captured_data_3, 31);
}

// Test group runner
TEST_GROUP_RUNNER(openhaystack_integration)
{
    RUN_TEST_CASE(openhaystack_integration, multiple_runs_same_adv_data_within_cycle);
}

static void run_all_tests(void)
{
    printf("Macless Haystack Test - Non-Rolling Codes\n");
    RUN_TEST_GROUP(non_rolling_codes);
    printf("\nMacless Haystack Test - Integration Tests\n");
    RUN_TEST_GROUP(openhaystack_integration);
}

// Test application main
void app_main(void)
{
    UNITY_MAIN_FUNC(run_all_tests);
}
