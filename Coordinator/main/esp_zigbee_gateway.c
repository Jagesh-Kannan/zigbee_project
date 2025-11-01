/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: LicenseRef-Included
 *
 * Zigbee Gateway Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include <fcntl.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_coexist.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_vfs_eventfd.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_zigbee_gateway.h"
#include "zb_config_platform.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "web_server.h"
#include "zb_devices.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zcl/esp_zigbee_zcl_on_off.h"






static const char *TAG = "ESP_ZB_GATEWAY";
#define HA_ESP_LIGHT_ENDPOINT 1




/* Note: Please select the correct console output port based on the development board in menuconfig */
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
esp_err_t esp_zb_gateway_console_init(void)
{
    esp_err_t ret = ESP_OK;
    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Enable non-blocking mode on stdin and stdout */
    fcntl(fileno(stdout), F_SETFL, O_NONBLOCK);
    fcntl(fileno(stdin), F_SETFL, O_NONBLOCK);

    usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    usb_serial_jtag_vfs_use_driver();
    uart_vfs_dev_register();
    return ret;
}
#endif

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee bdb commissioning");
}


void node_desc_req_callback(bool is_success, void *user_ctx)
{
    // This callback confirms the REQUEST was transmitted or failed, 
    // but the actual response data is handled by the ZDO signal handler.

    if (is_success) {
        ESP_LOGI(TAG, "Node Descriptor Request sent successfully.");
    } else {
        ESP_LOGE(TAG, "Node Descriptor Request transmission failed.");
    }
    
    // Note: You would typically use user_ctx here to pass context 
    // if you had multiple different ZDO requests.
}


void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
#if CONFIG_EXAMPLE_CONNECT_WIFI
#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE
        esp_coex_wifi_i154_enable();
#endif /* CONFIG_ESP_COEX_SW_COEXIST_ENABLE */
        ESP_RETURN_ON_FALSE(example_connect() == ESP_OK, , TAG, "Failed to connect to Wi-Fi");
        ESP_RETURN_ON_FALSE(esp_wifi_set_ps(WIFI_PS_MIN_MODEM) == ESP_OK, , TAG, "Failed to set Wi-Fi minimum modem power save type");
#endif /* CONFIG_EXAMPLE_CONNECT_WIFI */
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network formation");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            } else {
                esp_zb_bdb_open_network(180);
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t ieee_address;
            esp_zb_get_long_address(ieee_address);
            ESP_LOGI(TAG, "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     ieee_address[7], ieee_address[6], ieee_address[5], ieee_address[4],
                     ieee_address[3], ieee_address[2], ieee_address[1], ieee_address[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGI(TAG, "Restart network formation (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network steering started");
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        esp_zb_zdo_signal_device_annce_params_t *dev_annce_params =
            (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);

        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);

        uint16_t short_addr = dev_annce_params->device_short_addr;
        uint8_t ieee[8];
        memcpy(ieee, dev_annce_params->ieee_addr, sizeof(esp_zb_ieee_addr_t));


        //----- get device type -----
        char* device_name = send_device_info_req(short_addr);

        uint8_t endpoint = 10;  // Assuming endpoint 10
        uint8_t coordinator_ieee[8];
        esp_zb_get_long_address(coordinator_ieee);

        esp_zb_zdo_bind_req_param_t bind_req;
        memset(&bind_req, 0, sizeof(bind_req));

        memcpy(bind_req.src_address, ieee, sizeof(esp_zb_ieee_addr_t));
        bind_req.src_endp = endpoint;
        bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
        bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        memcpy(bind_req.dst_address_u.addr_long, coordinator_ieee, sizeof(esp_zb_ieee_addr_t));
        bind_req.dst_endp = ESP_ZB_GATEWAY_ENDPOINT;

        // esp_zb_zdo_bind_req(&bind_req);
        ESP_LOGI(TAG, "Binding request sent for device 0x%04x", short_addr);

        uint8_t eps[1] = { endpoint };
        zb_device_add(short_addr, ieee, eps, 1);

        ESP_LOGI(TAG, "New device: short 0x%04x added", short_addr);

        // Log the initiation of the interrogation
        ESP_LOGI(TAG, "New device announced (short: 0x%04hx). Retrieving Node Descriptor...", short_addr);

       zb_device_add(short_addr, ieee, NULL, 0); 
        ESP_LOGI(TAG, "New device announced (short: 0x%04hx). Retrieving descriptors...", short_addr);

        // 2. Initiate the Node Descriptor Request to get basic type information.
        esp_zb_zdo_node_desc_req_param_t node_desc_req;
        node_desc_req.dst_nwk_addr = short_addr;
        esp_zb_zdo_node_desc_req(&node_desc_req, NULL,NULL);




        // web_server_notify_device_paired(short_addr, device_name);

        break;



    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            } else {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
        ESP_LOGI(TAG, "Production configuration is %s", err_status == ESP_OK ? "ready" : "not present");
        esp_zb_set_node_descriptor_manufacturer_code(ESP_MANUFACTURER_CODE);
        break;
    
    // case ESP_ZB_ZDO_SIGNAL_NODE_DESC_RESPONSE:
       

    //    esp_zb_zdo_signal_device_node_desc_resp_param_t *desc_resp = 
    //         (esp_zb_zdo_signal_device_node_desc_resp_param_t *)esp_zb_app_signal_get_params(p_sg_p);
        
    //     if (desc_resp->zdo_status != ESP_ZB_ZDO_STATUS_SUCCESS) {
    //         ESP_LOGW(TAG, "Node Desc Response failed (Status: 0x%x)", desc_resp->zdo_status);
    //         break;
    //     }
        
    //     ESP_LOGI(TAG, "Node Desc Response OK for 0x%04hx. Requesting Active Endpoints...", desc_resp->src_addr);

    //     get_zb_active_devices(desc_resp);

    //     break;

        // case ESP_ZB_ZDO_SIGNAL_ACTIVE_EP_RESPONSE:

        //         esp_zb_zdo_signal_active_ep_resp_param_t *ep_resp = 
        //         (esp_zb_zdo_signal_active_ep_resp_param_t *)esp_zb_app_signal_get_params(p_sg_p);
            
        //     if (ep_resp->zdo_status != ESP_ZB_ZDO_STATUS_SUCCESS) {
        //         ESP_LOGW(TAG, "Active EP Response failed (Status: 0x%x)", ep_resp->zdo_status);
        //         break;
        //     }
            
        //     ESP_LOGI(TAG, "Active EP Response OK for 0x%04hx. Found %d endpoints.", ep_resp->src_addr, ep_resp->ep_count);

        //     // 1. Iterate through the active endpoint list
        //     for (int i = 0; i < ep_resp->ep_count; i++) {
        //         uint8_t endpoint = ep_resp->ep_list[i];
                
        //         // 2. Define and populate the Simple Descriptor Request structure
        //         esp_zb_zdo_simple_desc_req_param_t simple_desc_req;
        //         simple_desc_req.dst_addr = ep_resp->src_addr;
        //         simple_desc_req.nwk_addr = ep_resp->src_addr;
        //         simple_desc_req.endpoint = endpoint;
                
        //         // 3. Make the request
        //         if (esp_zb_zdo_simple_desc_req(&simple_desc_req) != ESP_OK) {
        //             ESP_LOGE(TAG, "Failed to send Simple Desc Request to 0x%04hx EP %d", ep_resp->src_addr, endpoint);
        //         }
        //     }
        //     break;

        // case ESP_ZB_ZDO_SIGNAL_SIMPLE_DESC_RESPONSE:

        //         esp_zb_zdo_signal_simple_desc_resp_param_t *simple_resp = 
        //         (esp_zb_zdo_signal_simple_desc_resp_param_t *)esp_zb_app_signal_get_params(p_sg_p);

        //     if (simple_resp->zdo_status != ESP_ZB_ZDO_STATUS_SUCCESS) {
        //         ESP_LOGW(TAG, "Simple Desc Response failed (Status: 0x%x)", simple_resp->zdo_status);
        //         break;
        //     }
            
        //     uint16_t short_addr = simple_resp->src_addr;
        //     uint16_t device_id = simple_resp->simple_descriptor.device_id;
        //     uint8_t endpoint = simple_resp->simple_descriptor.endpoint;
            
        //     // Find the device in your list and update its endpoints/type here
        //     // zb_device_t *device = zb_device_find_by_short(short_addr);
            
        //     ESP_LOGI(TAG, "Device 0x%04hx, EP %d is Device ID: 0x%04hx (Type: %s)", 
        //             short_addr, endpoint, device_id, 
        //             device_id == ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID ? "On/Off Light" : "Other");

        //     // Once you've aggregated all necessary info, you can notify the web server.
        //     // web_server_notify_device_paired(short_addr); // Update this function to include device_id/type

        //     break;

        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}


void wifi_init_sta(void)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Airtel-MyWifi - Jaki",
            .password = "jaki4220",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("wifi", "Wi-Fi initialization completed.");
}


static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = ESP_ZB_GATEWAY_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID,
        .app_device_version = 0,
    };

    esp_zb_attribute_list_t *basic_cluser = esp_zb_basic_cluster_create(NULL);
    esp_zb_basic_cluster_add_attr(basic_cluser, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_cluser, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluser, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_ep_list_add_gateway_ep(ep_list, cluster_list, endpoint_config);
    esp_zb_device_register(ep_list);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
    vTaskDelete(NULL);
}

void app_main(void)
{
   
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    ESP_ERROR_CHECK(esp_zb_gateway_console_init());
#endif
    zb_devices_init();

    ESP_ERROR_CHECK(web_server_start());

    xTaskCreate(esp_zb_task, "Zigbee_main", 8192, NULL, 5, NULL);
}
