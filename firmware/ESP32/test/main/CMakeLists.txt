idf_component_register(SRCS "main.c" 
                            "../../main/openhaystack_main.c"
                            "mocks/mock_esp_gap_ble_api.c"
                    INCLUDE_DIRS "." 
                                 "../../main"
                                 "mocks"
                    REQUIRES unity
                    PRIV_REQUIRES bt nvs_flash)

# Add linker flags to wrap BLE GAP functions with our mocks
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=esp_ble_gap_config_adv_data_raw"
    "-Wl,--wrap=esp_ble_gap_register_callback"
    "-Wl,--wrap=esp_ble_gap_set_rand_addr"
    "-Wl,--wrap=esp_ble_gap_start_advertising"
    "-Wl,--wrap=esp_ble_gap_stop_advertising"
)