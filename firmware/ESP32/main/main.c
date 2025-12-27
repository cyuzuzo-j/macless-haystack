#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "openhaystack.h"

static const char *TAG = "main";

/* Delay between advertisement. Advertisment will only be transmitted for a short period of time (20ms) and the device will go to sleep.
Higher delay = less power consumption, but more inaccurate tracking
 */
#define DELAY_IN_S 60

/* Define how often (long) a key will be reused after switching to the next one
This is for using less keys after all. The interval for one key is (DELAY_IN_S * REUSE_CYCLES => 60s * 30 cycles = changes key every 30 min)
Smaller number of cycles = key changes more often, but more keys needed.
 */
#define REUSE_CYCLES 30

void app_main(void)
{
    // Uncomment for debugging. Otherwise the serial will not have enough time to connect to PC
    // vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Starting Macless Haystack application");

    // Initialize OpenHaystack
    if (openhaystack_init() != 0) {
        ESP_LOGE(TAG, "Failed to initialize OpenHaystack");
        return;
    }

    // Run the beacon advertisement loop
    while (true) {
        openhaystack_run(DELAY_IN_S, REUSE_CYCLES);
    }
}
