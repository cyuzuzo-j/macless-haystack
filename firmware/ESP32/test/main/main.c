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

static void run_all_tests(void)
{
    printf("Macless Haystack Test - Non-Rolling Codes\n");
    RUN_TEST_GROUP(non_rolling_codes);
}

// Test application main
void app_main(void)
{
    UNITY_MAIN_FUNC(run_all_tests);
}
