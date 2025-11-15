/**
 * @file openhaystack.h
 * @brief Header file for OpenHaystack ESP32 firmware functions
 */

#ifndef OPENHAYSTACK_H
#define OPENHAYSTACK_H

#include <stdint.h>

/**
 * @brief Initialize the OpenHaystack system
 * 
 * This function initializes Bluetooth, loads keys, and prepares
 * the system for advertising.
 * 
 * @return 0 on success, non-zero on error
 */
int openhaystack_init(void);

/**
 * @brief Run one cycle of OpenHaystack beacon advertisement
 * 
 * This function advertises the beacon once, manages key rotation,
 * and prepares for deep sleep.
 */
void openhaystack_run(void);

/**
 * @brief Set the BLE address from a public key
 * 
 * This function generates a BLE address from a public key.
 * The address is deterministic - the same key always produces
 * the same address, enabling non-rolling code behavior.
 * 
 * @param addr Output buffer for the 6-byte BLE address
 * @param public_key Input 28-byte public key
 */
void set_addr_from_key(uint8_t addr[6], uint8_t *public_key);

/**
 * @brief Set the advertising payload from a public key
 * 
 * This function generates an advertising payload from a public key.
 * The payload is deterministic - the same key always produces
 * the same payload, enabling non-rolling code behavior.
 * 
 * @param payload Output buffer for the 31-byte advertising payload
 * @param public_key Input 28-byte public key
 */
void set_payload_from_key(uint8_t *payload, uint8_t *public_key);

/**
 * @brief Get the count of keys stored in the partition
 * 
 * @return Number of keys available, or 0 on error
 */
uint8_t get_key_count(void);

/**
 * @brief Load bytes from the key partition
 * 
 * @param dst Destination buffer
 * @param size Number of bytes to read
 * @param offset Offset in the partition
 * @return ESP_OK on success, error code otherwise
 */
int load_bytes_from_partition(uint8_t *dst, size_t size, int offset);

#endif // OPENHAYSTACK_H
