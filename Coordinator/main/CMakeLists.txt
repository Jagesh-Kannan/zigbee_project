idf_component_register(
    SRCS "color_utils.c" "esp_zigbee_gateway.c" "web_server.c" "zb_devices.c"
    PRIV_REQUIRES ieee802154 esp_driver_usb_serial_jtag esp_coex esp_netif
                 vfs esp_wifi nvs_flash esp_driver_uart esp_http_server
    INCLUDE_DIRS "."
)
target_add_binary_data(${COMPONENT_TARGET} "webInterface.html" TEXT)
