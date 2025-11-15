idf_component_register(SRCS "main.c" 
                            "../../main/openhaystack_main.c"
                    INCLUDE_DIRS "." 
                                 "../../main"
                    REQUIRES unity
                    PRIV_REQUIRES bt nvs_flash)