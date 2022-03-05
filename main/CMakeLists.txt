# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
set(srcs    
        "init.c"
        "adc.c"
        "connectWifi.c"
        "parser.c"
        "sensor.c"
        "button_adc.c"
        "button_gpio.c"
        "iot_button.c"
        "storage.c"
        "ota.c"
        "http_client.c"
        "common.c"  
        "gatt_server.c"
        "network.c"
        "main.c"  
        "wifi_test.c"
        "ble_test/esp_ibeacon_api.c"
		"ble_test/ble_test.c"
         )
idf_component_register(SRCS  "${srcs}"
                    INCLUDE_DIRS "." "./ble_test"
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
