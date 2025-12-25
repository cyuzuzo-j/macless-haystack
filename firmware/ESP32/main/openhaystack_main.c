#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "openhaystack.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_partition.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_random.h"

// --- MBEDTLS INCLUDES FOR ROLLING KEYS ---
#include "mbedtls/md.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

/* * UNCOMMENT THE LINE BELOW TO ENABLE ROLLING KEYS 
 * If commented out, the device uses the static keys from the partition.
 */
#define USE_ROLLING_KEYS

/* Delay between advertisement. Advertisment will only be transmitted for a short period of time (20ms) and the device will go to sleep.
Higher delay = less power consumption, but more inaccurate tracking
 */
#define DELAY_IN_S 60

/* Define how often (long) a key will be reused after switching to the next one
This is for using less keys after all. The interval for one key is (DELAY_IN_S * REUSE_CYCLES => 60s * 30 cycles = changes key every 30 min)
Smaller number of cycles = key changes more often, but more keys needed.
 */
#define REUSE_CYCLES 30

static const char *LOG_TAG = "macless_haystack";

/** Callback function for BT events */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

/** Random device address */
static esp_bd_addr_t rnd_addr = {0xFF, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

/** Advertisement payload */
static uint8_t adv_data[31] = {
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

/* https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html#_CPPv420esp_ble_adv_params_t */
static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min = 0x0020, // 20ms
    .adv_int_max = 0x0020, // 20ms
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// =============================================================================
// ROLLING KEY IMPLEMENTATION
// =============================================================================

#ifdef USE_ROLLING_KEYS

// NIST P-224 Curve Order (same as Python script)
static const char *P224_ORDER_HEX = "ffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d";

// Persistent State
RTC_DATA_ATTR uint8_t current_symmetric_key[32];
RTC_DATA_ATTR uint8_t master_private_key[28]; // Loaded from NVS
RTC_DATA_ATTR bool rolling_keys_initialized = false;
RTC_DATA_ATTR uint8_t current_public_key[28];

/**
 * @brief SHA256 Implementation (Ported from provided C++ snippet to C)
 */
void generateSHA256(const uint8_t *data, size_t len, uint8_t *output) {
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, data, len);
    mbedtls_md_finish(&ctx, output);
    mbedtls_md_free(&ctx);
}

/**
 * @brief ANSI X.963 KDF using SHA-256 (matches Python implementation)
 */
void ansi_x963_kdf(const uint8_t *input_key, size_t input_len, 
                   const char *shared_info, 
                   size_t output_len, uint8_t *output) {
    
    uint32_t counter = 1;
    size_t generated = 0;
    uint8_t hash[32];
    uint8_t counter_be[4];

    while (generated < output_len) {
        // Pack counter as big-endian
        counter_be[0] = (counter >> 24) & 0xFF;
        counter_be[1] = (counter >> 16) & 0xFF;
        counter_be[2] = (counter >> 8) & 0xFF;
        counter_be[3] = counter & 0xFF;

        mbedtls_md_context_t ctx;
        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
        mbedtls_md_starts(&ctx);
        
        // Hash(InputKey || Counter || SharedInfo)
        mbedtls_md_update(&ctx, input_key, input_len);
        mbedtls_md_update(&ctx, counter_be, 4);
        mbedtls_md_update(&ctx, (const uint8_t*)shared_info, strlen(shared_info));
        
        mbedtls_md_finish(&ctx, hash);
        mbedtls_md_free(&ctx);

        size_t copy_len = (output_len - generated) < 32 ? (output_len - generated) : 32;
        memcpy(output + generated, hash, copy_len);
        generated += copy_len;
        counter++;
    }
}

/**
 * @brief Derive Public Key from Private Scalar
 * Adapted from provided implementation
 */
void derive_public_key_bytes(const uint8_t *priv_key,
                             size_t priv_len,
                             uint8_t *pub_key_out)
{
    int ret;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
    mbedtls_mpi d;

    uint8_t buf[1 + 2 * 28]; // 0x04 || X || Y for P-224
    size_t olen = 0;

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q);
    mbedtls_mpi_init(&d);

    /* Load curve */
    // IF THIS FAILS: It means P-224 is disabled in make menuconfig
    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP224R1);
    if (ret != 0) {
        ESP_LOGE(LOG_TAG, "mbedtls_ecp_group_load failed: -0x%04x (Is P-224 enabled in config?)", -ret);
        goto cleanup;
    }

    /* Load private scalar */
    // FIX: Ensure we don't read more than 28 bytes for P-224.
    // Rolling keys (SHA256) are 32 bytes, which causes "Invalid Key" (value > curve order) 
    // if we read all 32 bytes.
    size_t effective_len = priv_len;
    if (effective_len > 28) {
        ESP_LOGW(LOG_TAG, "Input key length %d > 28. Truncating to 28 bytes for P-224.", (int)priv_len);
        effective_len = 28;
    }

    ret = mbedtls_mpi_read_binary(&d, priv_key, effective_len);
    if (ret != 0) {
        ESP_LOGE(LOG_TAG, "mbedtls_mpi_read_binary failed: -0x%04x", -ret);
        goto cleanup;
    }

    /* Diagnostic: dump private scalar bytes and compare to group order */
    {
        uint8_t d_dump[28];
        uint8_t n_dump[28];
        memset(d_dump, 0, sizeof(d_dump));
        memset(n_dump, 0, sizeof(n_dump));
        mbedtls_mpi_write_binary(&d, d_dump, sizeof(d_dump));
        mbedtls_mpi_write_binary(&grp.N, n_dump, sizeof(n_dump));
        char d_str[28 * 2 + 1];
        char n_str[28 * 2 + 1];
        for (int i = 0; i < 28; i++) sprintf(&d_str[i * 2], "%02x", d_dump[i]);
        d_str[28 * 2] = '\0';
        for (int i = 0; i < 28; i++) sprintf(&n_str[i * 2], "%02x", n_dump[i]);
        n_str[28 * 2] = '\0';
        ESP_LOGI(LOG_TAG, "Private scalar (big-endian): %s", d_str);
        ESP_LOGI(LOG_TAG, "Curve order N (big-endian): %s", n_str);
        int cmp = mbedtls_mpi_cmp_mpi(&d, &grp.N);
        if (cmp >= 0) {
            ESP_LOGE(LOG_TAG, "Diagnostic: private scalar >= N (cmp=%d)", cmp);
        }
    }

    /* --- DIAGNOSTIC CHECKS --- */

    // Check 1: Is the key exactly ZERO?
    if (mbedtls_mpi_cmp_int(&d, 0) == 0) {
        ESP_LOGE(LOG_TAG, "CRITICAL ERROR: Private key is ZERO!");
        ret = MBEDTLS_ERR_ECP_INVALID_KEY; 
        goto cleanup;
    }

    // Check 2: Is the key too large? (d >= N)
    if (mbedtls_mpi_cmp_mpi(&d, &grp.N) >= 0) {
        ESP_LOGE(LOG_TAG, "CRITICAL ERROR: Private key is larger than Curve Order!");
        ESP_LOGE(LOG_TAG, "This usually means the Byte Order (Endianness) is wrong.");
        ret = MBEDTLS_ERR_ECP_INVALID_KEY;
        goto cleanup;
    }
    
    /* ----------------------------- */

    /* Q = d * G */
    /* Use CTR-DRBG for RNG/blinding to avoid build-config related failures */
    {
        int rng_ret;
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context ctr_drbg;
        const char *pers = "openhaystack_ecp";

        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        rng_ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                        (const unsigned char *)pers, strlen(pers));
        if (rng_ret != 0) {
            ESP_LOGE(LOG_TAG, "CTR-DRBG seed failed: -0x%04x", -rng_ret);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            mbedtls_entropy_free(&entropy);
            goto cleanup;
        }

        ret = mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, mbedtls_ctr_drbg_random, &ctr_drbg);
        if (ret != 0) {
            ESP_LOGE(LOG_TAG, "mbedtls_ecp_mul failed: -0x%04x", -ret);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            mbedtls_entropy_free(&entropy);
            goto cleanup;
        }

        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
    }

    /* Export uncompressed public key: 04 || X || Y */
    ret = mbedtls_ecp_point_write_binary(
        &grp,
        &Q,
        MBEDTLS_ECP_PF_UNCOMPRESSED,
        &olen,
        buf,
        sizeof(buf)
    );
    if (ret != 0) {
        ESP_LOGE(LOG_TAG, "mbedtls_ecp_point_write_binary failed: -0x%04x", -ret);
        goto cleanup;
    }

    /* Copy X coordinate only (skip 0x04 prefix) */
    memcpy(pub_key_out, buf + 1, 28);
    ESP_LOGI(LOG_TAG, "Public key derivation successful");

cleanup:
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&Q);
    mbedtls_ecp_group_free(&grp);
}
/**
 * @brief Main rolling key logic
 * 1. Rotate Symmetric Key
 * 2. Generate u, v scalars
 * 3. Calculate new Private Key
 * 4. Derive new Public Key
 */
void roll_key_and_update_state() {
    uint8_t next_sym_key[32];
    uint8_t diversify_material[72];
    uint8_t u_bytes[36];
    uint8_t v_bytes[36];
    uint8_t rolling_priv_bytes[28];
    ESP_LOGI(LOG_TAG, "Rolling keys...");
    ESP_LOGI(LOG_TAG, "Current Symmetric Key:");
    {
        char key_str[32 * 2 + 1];
        for (int i = 0; i < 32; i++) {
            sprintf(&key_str[i * 2], "%02x", current_symmetric_key[i]);
        }
        key_str[32 * 2] = '\0';
        ESP_LOGI(LOG_TAG, "%s", key_str);
    }
    // 1. Update Symmetric Key: SK_new = KDF(SK_old, "update", 32)
    ansi_x963_kdf(current_symmetric_key, 32, "update", 32, next_sym_key);
    // Update global state immediately
    memcpy(current_symmetric_key, next_sym_key, 32);
    ESP_LOGI(LOG_TAG, "New Symmetric Key:");
    {
        char key_str[32 * 2 + 1];
        for (int i = 0; i < 32; i++) {
            sprintf(&key_str[i * 2], "%02x", current_symmetric_key[i]);
        }
        key_str[32 * 2] = '\0';
        ESP_LOGI(LOG_TAG, "%s", key_str);
    }


    // 2. Derive u, v: KDF(SK_new, "diversify", 72)
    ansi_x963_kdf(next_sym_key, 32, "diversify", 72, diversify_material);
    memcpy(u_bytes, diversify_material, 36);
    memcpy(v_bytes, diversify_material + 36, 36);
    // Debug print u, v
    ESP_LOGI(LOG_TAG, "u:");
    {
        char u_str[36 * 2 + 1];
        for (int i = 0; i < 36; i++) {
            sprintf(&u_str[i * 2], "%02x", u_bytes[i]);
        }
        u_str[36 * 2] = '\0';
        ESP_LOGI(LOG_TAG, "%s", u_str);
    }
    ESP_LOGI(LOG_TAG, "v:");
    {
        char v_str[36 * 2 + 1];
        for (int i = 0; i < 36; i++) {
            sprintf(&v_str[i * 2], "%02x", v_bytes[i]);
        }
        v_str[36 * 2] = '\0';
        ESP_LOGI(LOG_TAG, "%s", v_str);
    }

    // 3. Math: d_i = (d_0 * u + v) mod n
    mbedtls_mpi d_0, u, v, n, d_i, temp;
    mbedtls_mpi_init(&d_0); mbedtls_mpi_init(&u); mbedtls_mpi_init(&v);
    mbedtls_mpi_init(&n);   mbedtls_mpi_init(&d_i); mbedtls_mpi_init(&temp);

    mbedtls_mpi_read_string(&n, 16, P224_ORDER_HEX);

    // Use the variable loaded from NVS instead of hardcoded const
    mbedtls_mpi_read_binary(&d_0, master_private_key, 28);
    mbedtls_mpi_read_binary(&u, u_bytes, 36);
    mbedtls_mpi_read_binary(&v, v_bytes, 36);

    // temp = d_0 * u
    mbedtls_mpi_mul_mpi(&temp, &d_0, &u);
    // d_i = temp + v
    mbedtls_mpi_add_mpi(&d_i, &temp, &v);
    // d_i = d_i mod n
    mbedtls_mpi_mod_mpi(&d_i, &d_i, &n);

    // Export result
    mbedtls_mpi_write_binary(&d_i, rolling_priv_bytes, 28);
    ESP_LOGI(LOG_TAG, "New Private Key:");
    {
        char key_str[28 * 2 + 1];
        for (int i = 0; i < 28; i++) {
            sprintf(&key_str[i * 2], "%02x", rolling_priv_bytes[i]);
        }
        key_str[28 * 2] = '\0';
        ESP_LOGI(LOG_TAG, "%s", key_str);
    }

    // Cleanup MPI
    mbedtls_mpi_free(&d_0); mbedtls_mpi_free(&u); mbedtls_mpi_free(&v);
    mbedtls_mpi_free(&n);   mbedtls_mpi_free(&d_i); mbedtls_mpi_free(&temp);

    // 4. Derive Public Key
    derive_public_key_bytes(rolling_priv_bytes, 28, current_public_key);
    ESP_LOGI(LOG_TAG, "New Public Key:");
    {
        char key_str[28 * 2 + 1];
        for (int i = 0; i < 28; i++) {
            sprintf(&key_str[i * 2], "%02x", current_public_key[i]);
        }
        key_str[28 * 2] = '\0';
        ESP_LOGI(LOG_TAG, "%s", key_str);
    }
}

#endif // USE_ROLLING_KEYS

// =============================================================================
// EXISTING IMPLEMENTATION
// =============================================================================

int load_bytes_from_partition(uint8_t *dst, size_t size, int offset)
{
    const esp_partition_t *keypart = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, "key");
    if (keypart == NULL)
    {
        ESP_LOGE(LOG_TAG, "Could not find key partition");
        return 1;
    }
    esp_err_t status;
    status = esp_partition_read(keypart, offset, dst, size);
    if (status != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Could not read key from partition: %s", esp_err_to_name(status));
    }
    return status;
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&ble_adv_params);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(LOG_TAG, "advertising start failed: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(LOG_TAG, "advertising has started.");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(LOG_TAG, "adv stop failed: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(LOG_TAG, "stop adv successfully");
        }
        break;
    default:
        break;
    }
}

void set_addr_from_key(esp_bd_addr_t addr, uint8_t *public_key)
{
    addr[0] = public_key[0] | 0b11000000;
    addr[1] = public_key[1];
    addr[2] = public_key[2];
    addr[3] = public_key[3];
    addr[4] = public_key[4];
    addr[5] = public_key[5];
}

void set_payload_from_key(uint8_t *payload, uint8_t *public_key)
{
    /* copy last 22 bytes */
    memcpy(&payload[7], &public_key[6], 22);
    /* append two bits of public key */
    payload[29] = public_key[0] >> 6;
}

uint8_t get_key_count()
{
    uint8_t keyCount[1];
    if (load_bytes_from_partition(keyCount, sizeof(keyCount), 0) != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Could not read the key count, stopping.");
        return 0;
    }
    ESP_LOGE(LOG_TAG, "Found %i keys", keyCount[0]);
    return keyCount[0];
}

static uint8_t public_key[28];
RTC_DATA_ATTR uint8_t key_count;
RTC_DATA_ATTR uint8_t key_index;
RTC_DATA_ATTR uint8_t cycle = 0;

int openhaystack_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_bluedroid_enable();

    // Check reset cause
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        #ifdef USE_ROLLING_KEYS
            ESP_LOGI(LOG_TAG, "OpenHaystack Initializing in ROLLING KEY mode");

            // Load initial rolling keys from NVS storage

            // Offset 1: Master Symmetric Key (32 bytes)
            if (load_bytes_from_partition(current_symmetric_key, 32, 1) != ESP_OK) {
                ESP_LOGE(LOG_TAG, "Failed to load Master Symmetric Key from NVS");
                return 1;
            }
            // Offset 32: Master Private Key (28 bytes)
            if (load_bytes_from_partition(master_private_key, 28, 33) != ESP_OK) {
                ESP_LOGE(LOG_TAG, "Failed to load Master Private Key from NVS");
                return 1;
            }

            rolling_keys_initialized = true;
            ESP_LOGI(LOG_TAG, "using private key with start %02x %02x", master_private_key[0], master_private_key[1]);
            ESP_LOGI(LOG_TAG, "using symetric key with start %02x %02x", current_symmetric_key[0], current_symmetric_key[1]);

            // Generate the first rolling key and public key
            roll_key_and_update_state();
        #else
            ESP_LOGI(LOG_TAG, "OpenHaystack Initializing in STATIC mode");
            /* Start with a random index */
            key_count = get_key_count();
            if (key_count == 0) {
                return 1;
            }
            key_index = (esp_random() % key_count);
            ESP_LOGI(LOG_TAG, "OpenHaystack initialized with %d keys", key_count);
        #endif
    }

    return 0;
}

void openhaystack_run(void)
{
    esp_err_t status;

    #ifdef USE_ROLLING_KEYS
        // Logic for Rolling Keys
        // If we woke up from sleep, 'cycle' has been incremented below.
        // We just use 'current_public_key' which is stored in RTC.
        memcpy(public_key, current_public_key, 28);
        ESP_LOGI(LOG_TAG, "Using Rolling Key");
    #else
        // Logic for Static Keys
        int address = 1 + (key_index * sizeof(public_key));
        ESP_LOGI(LOG_TAG, "Loading key with index %d at address %d", key_index, address);
        if (load_bytes_from_partition(public_key, sizeof(public_key), address) != ESP_OK)
        {
            ESP_LOGE(LOG_TAG, "Could not read the key, stopping.");
            return;
        }
    #endif

    ESP_LOGI(LOG_TAG, "using key with start %02x %02x", public_key[0], public_key[1]);
    set_addr_from_key(rnd_addr, public_key);
    set_payload_from_key(adv_data, public_key);

    ESP_LOGI(LOG_TAG, "using device address: %02x %02x %02x %02x %02x %02x", rnd_addr[0], rnd_addr[1], rnd_addr[2], rnd_addr[3], rnd_addr[4], rnd_addr[5]);
    
    // register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "gap register error: %s", esp_err_to_name(status));
        return;
    }

    if ((status = esp_ble_gap_set_rand_addr(rnd_addr)) != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "couldn't set random address: %s", esp_err_to_name(status));
        return;
    }
    if ((esp_ble_gap_config_adv_data_raw((uint8_t *)&adv_data, sizeof(adv_data))) != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "couldn't configure BLE adv: %s", esp_err_to_name(status));
        return;
    }
    
    ESP_LOGI(LOG_TAG, "Sending beacon");
    vTaskDelay(10);
    esp_ble_gap_stop_advertising(); // Stop immediately after first beacon

    // CYCLE MANAGEMENT
    if (cycle >= REUSE_CYCLES)
    {
        ESP_LOGI(LOG_TAG, "Max cycles %d are reached. Changing key ", cycle);
        
        #ifdef USE_ROLLING_KEYS
            roll_key_and_update_state();
            ESP_LOGI(LOG_TAG, "Rolled to new key.");
        #else
            key_index = (key_index + 1) % key_count; // Back to zero if out of range
        #endif
        
        cycle = 0;
    }
    else
    {
        ESP_LOGI(LOG_TAG, "Current cycle is %d. Reusing key. ", cycle);
        cycle++;
    }

    ESP_ERROR_CHECK(esp_bluedroid_disable());
    ESP_ERROR_CHECK(esp_bluedroid_deinit());
    ESP_ERROR_CHECK(esp_bt_controller_disable());
    ESP_ERROR_CHECK(esp_bt_controller_deinit());

    vTaskDelay(10);
    ESP_LOGI(LOG_TAG, "Going to sleep");
    vTaskDelay(10);
    esp_sleep_enable_timer_wakeup(DELAY_IN_S * 1000000); // sleep
    esp_deep_sleep_start();
}