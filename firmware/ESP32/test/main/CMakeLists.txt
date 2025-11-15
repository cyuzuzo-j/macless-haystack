idf_component_register("main.c" 
    SRCS "main.c"
         "../../main/openhaystack_main.c"
         "shims/esp_gap_ble_test_shim.c"
         "shims/esp_sleep_test_shim.c"
         "shims/esp_gap_ble_register_cb_test_shim.c"
         "shims/esp_gap_ble_set_rand_addr_test_shim.c"
         "shims/esp_bt_teardown_test_shim.c"
    INCLUDE_DIRS "."
                 "../../main"
    PRIV_REQUIRES bt nvs_flash unity
)

target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=esp_ble_gap_config_adv_data_raw"
    "-Wl,--wrap=esp_sleep_enable_timer_wakeup"
    "-Wl,--wrap=esp_deep_sleep_start"
    "-Wl,--wrap=esp_ble_gap_register_callback"
    "-Wl,--wrap=esp_ble_gap_set_rand_addr"
    "-Wl,--wrap=esp_bluedroid_disable"
    "-Wl,--wrap=esp_bluedroid_deinit"
    "-Wl,--wrap=esp_bt_controller_disable"
    "-Wl,--wrap=esp_bt_controller_deinit"
)
