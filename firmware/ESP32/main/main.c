#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "openhaystack.h"

static const char *TAG = "main";

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
        openhaystack_run();
        // Note: openhaystack_run() will put the device to deep sleep
        // and this loop will restart after wakeup
    }
}
