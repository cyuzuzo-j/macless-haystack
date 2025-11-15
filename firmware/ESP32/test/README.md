# ESP32 Firmware Test Suite

This directory contains unit tests for the OpenHaystack ESP32 firmware.

## Overview

The test suite verifies that the firmware correctly implements non-rolling code behavior, which is essential for the Find My network integration.

## Non-Rolling Codes

The firmware uses **non-rolling codes**, meaning the same advertising key is reused for multiple advertisement cycles before switching to the next key. This is controlled by the `REUSE_CYCLES` parameter (default: 30 cycles).

**Key characteristics verified by tests:**
- The same public key always generates the same BLE address
- The same public key always generates the same advertising payload
- Keys are reused across multiple cycles (non-rolling behavior)
- Different keys produce different addresses and payloads

## Test Structure

### `test_non_rolling_codes.c`

This test file contains unit tests that verify:

1. **Deterministic address generation** - Same key produces same address every time
2. **Deterministic payload generation** - Same key produces same payload every time
3. **Address format** - Proper bit patterns and key material inclusion
4. **Payload content** - Correct encoding of public key material
5. **Multiple reuse** - Calling functions multiple times with same key produces identical results
6. **Key differentiation** - Different keys produce different outputs

## Running Tests

### Using PlatformIO

The tests use the Unity test framework. To run tests:

```bash
# Build and run tests
pio test -e esp32dev

# Or run specific test
pio test -e esp32dev -f test_non_rolling_codes
```

### Using ESP-IDF

If using native ESP-IDF:

```bash
# Configure for testing
idf.py -C test menuconfig

# Build and run
idf.py -C test build flash monitor
```

## Test Functions

### Core Non-Rolling Code Tests

- `test_set_addr_from_key_is_deterministic()` - Verifies address generation is deterministic
- `test_set_payload_from_key_is_deterministic()` - Verifies payload generation is deterministic
- `test_multiple_calls_same_key_same_output()` - **Main test proving non-rolling behavior**

### Format and Content Tests

- `test_set_addr_from_key_format()` - Validates BLE address format
- `test_set_payload_from_key_content()` - Validates payload structure

### Differentiation Tests

- `test_different_keys_different_addresses()` - Ensures different keys produce different addresses
- `test_different_keys_different_payloads()` - Ensures different keys produce different payloads

## Expected Results

All tests should pass, confirming:

✅ Non-rolling code behavior is correctly implemented  
✅ Same key produces consistent advertisements across cycles  
✅ Advertisement format matches Find My network requirements  
✅ Key material is properly encoded in addresses and payloads

## Adding New Tests

To add new tests:

1. Add test functions to `test_non_rolling_codes.c` following the pattern:
```c
void test_your_new_test(void)
{
    // Setup
    // Execute
    // Assert using TEST_ASSERT_* macros
}
```

2. Register the test in `app_main()`:
```c
RUN_TEST(test_your_new_test);
```

3. Rebuild and run tests

## Continuous Integration

Tests can be integrated into CI/CD pipelines. Example GitHub Actions workflow:

```yaml
- name: Run ESP32 firmware tests
  run: |
    cd firmware/ESP32
    pio test -e esp32dev
```
