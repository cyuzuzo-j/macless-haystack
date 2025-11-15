idf_component_register(SRCS "main.c" 
                            "../../main/openhaystack_main.c"
                            "mocks/mock_esp_gap_ble_api.c"
                    INCLUDE_DIRS "." 
                                 "../../main"
                                 "mocks"
                    REQUIRES unity cmock
                    PRIV_REQUIRES bt nvs_flash)