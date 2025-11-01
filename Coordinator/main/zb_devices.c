#include "zb_devices.h"
#include <string.h>
#include <stdio.h>
#include "esp_zigbee_gateway.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zcl/esp_zigbee_zcl_on_off.h"
#include "web_server.h"
#include "zcl/esp_zigbee_zcl_color_control.h"
#include "color_utils.h"

static zb_device_t devices[MAX_DEVICES];
static const char *TAG = "ESP_ZB_GATEWAY";
static char* device_type = "Unknown";

typedef struct {
    uint16_t short_addr;
    // Add other info needed later, like the endpoint ID, if necessary
} custom_user_context_t;

void zb_devices_init(void)
{
    for (int i = 0; i < MAX_DEVICES; ++i) devices[i].used = false;
}

zb_device_t *zb_device_find_by_short(uint16_t short_addr)
{
    for (int i = 0; i < MAX_DEVICES; ++i) {
        if (devices[i].used && devices[i].short_addr == short_addr) return &devices[i];
    }
    return NULL;
}

zb_device_t *zb_device_add(uint16_t short_addr, const uint8_t ieee[8], uint8_t *eps, uint8_t num_eps)
{
    zb_device_t *existing = zb_device_find_by_short(short_addr);
    if (existing) return existing;

    for (int i = 0; i < MAX_DEVICES; ++i) {
        if (!devices[i].used) {
            devices[i].used = true;
            devices[i].short_addr = short_addr;
            if (ieee) memcpy(devices[i].ieee, ieee, 8);
            else memset(devices[i].ieee, 0, 8);
            devices[i].num_endpoints = num_eps > 8 ? 8 : num_eps;
            if (eps && num_eps) memcpy(devices[i].endpoints, eps, devices[i].num_endpoints);
            else memset(devices[i].endpoints, 0, sizeof(devices[i].endpoints));
            snprintf(devices[i].name, sizeof(devices[i].name), "zb_%04x", short_addr);
            return &devices[i];
        }
    }
    return NULL;
}

char* get_device_name_by_id(uint16_t device_id){

    switch (device_id){

        case ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID:
            return "General On/Off switch";
            break;
        case  ESP_ZB_HA_LEVEL_CONTROL_SWITCH_DEVICE_ID:
            return "Level Control Switch";
            break;
        case  ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID  :
            return "General On/Off output";
            break;
        case ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID:
            return "Simple Sensor device";
            break;
        case  ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID :
            return "On/Off Light Device";
            break;
        case ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID:
            return "Dimmable Light Device";
            break;
        case ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID  :
            return "Color Dimmable Light Device";
            break;
        case ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID:
            return "Temperature Sensor";
            break;
        default:
             return "Unknown device";
             break;
        

    }
}

void node_desc_callback(esp_zb_zdp_status_t zdo_status, uint16_t short_addr, esp_zb_af_node_desc_t *node_desc, void *user_ctx){
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {

        custom_user_context_t *context = (custom_user_context_t *)user_ctx;

        ESP_LOGI(TAG, "Node Desc Response from 0x%04hx received.", context->short_addr);
         ESP_LOGI(TAG, "Node Desc value----", node_desc);

    } else {
        // ESP_LOGW(TAG, "Node Desc Request to 0x%04hx failed with status: 0x%x", context->short_addr, zdo_status);
    }
}

void node_type_callback(esp_zb_zdp_status_t zdo_status, esp_zb_af_simple_desc_1_1_t *simple_desc, void *user_ctx){
     if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
       
         custom_user_context_t *context = (custom_user_context_t *)user_ctx;
 ESP_LOGW(TAG, "vcallback 0x%04hx", context->short_addr);
         ESP_LOGI(TAG, "Node type value----0x%04hx", simple_desc->app_device_id);
         device_type =  get_device_name_by_id( simple_desc->app_device_id);
        ESP_LOGI(TAG, "Node name value----%s", device_type);
         
        web_server_notify_device_paired(context->short_addr, device_type);

    } else {
        ESP_LOGW(TAG, "Node Desc Request  failed with status: 0x%x", zdo_status);
        
    }
}


char* get_connectetd_device_type(uint16_t short_addr){

     uint8_t endpoint = 10;

      ESP_LOGW(TAG, "get connected devoce shprt address 0x%04hx", short_addr);
     
     custom_user_context_t *context_ptr = (custom_user_context_t *)malloc(sizeof(custom_user_context_t));

     context_ptr->short_addr = short_addr;

    esp_zb_zdo_simple_desc_req_param_t simple_desc_req;
        simple_desc_req.addr_of_interest = short_addr;
        simple_desc_req.endpoint = endpoint;
        
        // 3. Make the request
    esp_zb_zdo_simple_desc_req(&simple_desc_req,node_type_callback, (void *)context_ptr);

    return device_type;
}

char* send_device_info_req(uint16_t short_addr){

 ESP_LOGW(TAG, "send device shprt address 0x%04hx", short_addr);

     custom_user_context_t  context;
          context.short_addr = short_addr;
          

    // 1. Define and populate the Node Descriptor Request structure
    esp_zb_zdo_node_desc_req_param_t node_desc_req;

    // The short address of the device we are interrogating
    node_desc_req.dst_nwk_addr = short_addr;

    // ESP_LOGE(" IITIATING DEVICE DESC REQUEST")
    // 2. Make the request
    esp_zb_zdo_node_desc_req(&node_desc_req,node_desc_callback, (void *)&context);
     return get_connectetd_device_type(short_addr);
        
}


// void get_zb_active_devices(esp_zb_zdo_signal_device_node_desc_resp_param_t desc_resp){

//     esp_zb_zdo_active_ep_req_param_t active_ep_req;
//     active_ep_req.dst_addr = desc_resp->src_addr;
//     active_ep_req.nwk_addr = desc_resp->src_addr;

//     // 2. Make the request
//     if (esp_zb_zdo_active_ep_req(&active_ep_req) != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to send Active EP Request to 0x%04hx", desc_resp->src_addr);
//     }
// }



void toggle_end_device(uint16_t short_addr){

     zb_device_t *device = zb_device_find_by_short(short_addr);
    
    if (device == NULL || device->num_endpoints == 0) {
        ESP_LOGE(TAG, "Device 0x%04hx not found or has no endpoints.", short_addr);
        return;
    }
    
    // For simplicity, we use the first discovered endpoint.
    // In a production system, you would check which endpoint supports the ON/OFF cluster.
    uint8_t endpoint = device->endpoints[0]; 


            esp_zb_zcl_on_off_cmd_t cmd_req = {
            // 1. Initialize the nested basic command structure
            .zcl_basic_cmd = {
                // Addressing Union (requires braces around the union initializer)
                .dst_addr_u = {
                    .addr_short = short_addr,
                },
                // Endpoints are inside basic_cmd_t
                .dst_endpoint = endpoint,
                .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
                // The address_mode is NOT here (as per your error)
                // If 'cluster_id' is required here, add it: .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                
            }, 
            // 2. Initialize the top-level 'on_off_cmd_t' fields
            
            // 'address_mode' is a top-level field (based on the first error)
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
            
            // 'on_off_cmd_id' is a top-level field (based on your structure typedef)
            .on_off_cmd_id =  ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID, 
        };

        esp_zb_zcl_on_off_cmd_req(&cmd_req);
}


/**
 * @brief HTTP handler for Level Control (Dimming).
 * URI: PUT /api/setDeviceLevel?sad=<short_addr>&level=<0-254>
 */
bool  set_zb_device_level_handler(uint16_t short_addr, uint8_t level)
{

    zb_device_t *device = zb_device_find_by_short(short_addr);
    uint8_t endpoint = device->endpoints[0]; 

    ESP_LOGI(TAG, "Setting level for 0x%04hx to %d", short_addr, level);

    
     esp_zb_zcl_move_to_level_cmd_t cmd_req = {
        // --- 1. Basic Command Info ---
        .zcl_basic_cmd = {
                // Addressing Union (requires braces around the union initializer)
                .dst_addr_u = {
                    .addr_short = short_addr,
                },
                // Endpoints are inside basic_cmd_t
                .dst_endpoint = endpoint,
                .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
                // The address_mode is NOT here (as per your error)
                // If 'cluster_id' is required here, add it: .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                
            }, 
        
        // --- 2. Addressing Mode ---
        // This top-level field must also be set to match the address type.
         .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 

        // --- 3. Level Payload ---
        .level = level,
        
        // --- 4. Transition Time ---
        .transition_time = 0, 
    };


    uint8_t tsn = esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(&cmd_req);

    if (tsn > 0) {
        // The TSN is non-zero, indicating the command request was queued successfully.
        return true;
    }

    return false;

            
}


static bool send_saturation_command(uint16_t short_addr, uint8_t endpoint, uint8_t saturation)
{
    esp_zb_zcl_color_move_to_saturation_cmd_t cmd_sat = { 
         .zcl_basic_cmd = {
                // Addressing Union (requires braces around the union initializer)
                .dst_addr_u = {
                    .addr_short = short_addr,
                },
                // Endpoints are inside basic_cmd_t
                .dst_endpoint = endpoint,
                .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
                // The address_mode is NOT here (as per your error)
                // If 'cluster_id' is required here, add it: .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                
            }, 
            // --- 2. Addressing Mode ---
        // This top-level field must also be set to match the address type.
         .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 

         
            .saturation = saturation,
              .transition_time = 0
    };


    
    return esp_zb_zcl_color_move_to_saturation_cmd_req(&cmd_sat) > 0;
}

bool esp_zb_util_set_hue_value_with_on_off(uint16_t short_addr, uint8_t hue)
{

     zb_device_t *device = zb_device_find_by_short(short_addr);
    uint8_t endpoint = device->endpoints[0]; 

      ESP_LOGI(TAG, "Setting saturation for 0x%04hx to %d", short_addr, hue);

      ColorXY xy_coords = get_xy_from_hue(hue);

    // Note: The structure for Color Control commands is likely 'esp_zb_zcl_color_move_to_hue_cmd_s' 
    // from the header file not provided, but we can infer the content.
    esp_zb_zcl_color_step_hue_cmd_t cmd_req = {
        .zcl_basic_cmd = {
                // Addressing Union (requires braces around the union initializer)
                .dst_addr_u = {
                    .addr_short = short_addr,
                },
                // Endpoints are inside basic_cmd_t
                .dst_endpoint = endpoint,
                .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
                // The address_mode is NOT here (as per your error)
                // If 'cluster_id' is required here, add it: .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                
            }, 
            // --- 2. Addressing Mode ---
        // This top-level field must also be set to match the address type.
         .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 

       .step_mode = 0x00,         // Step up
    .step_size = 30,   

        .transition_time = 0, // Set immediately
    };

    // The Color Control cluster might have a 'with on/off' variant, but without the full 
    // list, we use the standard move command, assuming the device is already on or 
    // a separate ON command will be sent by the application logic.
    // uint8_t tsn =  esp_zb_zcl_color_step_hue_cmd_req(&cmd_req);


    /* Color Control Cluster Commands */

/**
 * @brief esp_zb_zcl_color_move_to_hue_cmd_t
 */
esp_zb_zcl_color_move_to_hue_cmd_t cmd_req5 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .hue = hue, // Target Hue (128)
    .direction = 0x00, // Shortest path
    .transition_time = 15, // 1.5 seconds
};

/**
 * @brief esp_zb_zcl_color_move_hue_cmd_t
 */
esp_zb_zcl_color_move_hue_cmd_t cmd_req6 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .move_mode = 0x01, // Move Up (incrementing hue)
    .rate = 0x05, // 5 units/second
};

/**
 * @brief esp_zb_zcl_color_step_hue_cmd_t
 */
esp_zb_zcl_color_step_hue_cmd_t cmd_req7 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .step_mode = 0x01, // Step Up (incrementing hue)
    .step_size = 0x20, // Step size of 32
    .transition_time = 10, // 1.0 seconds
};

/**
 * @brief esp_zb_zcl_color_move_to_saturation_cmd_t
 */
esp_zb_zcl_color_move_to_saturation_cmd_t cmd_req8 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .saturation = 0xFF, // Target Saturation (Max)
    .transition_time = 5, // 0.5 seconds
};

/**
 * @brief esp_zb_zcl_color_move_saturation_cmd_t
 */
esp_zb_zcl_color_move_saturation_cmd_t cmd_req9 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .move_mode = 0x01, // Move Up (incrementing saturation)
    .rate = 0x0A, // 10 units/second
};

/**
 * @brief esp_zb_zcl_color_step_saturation_cmd_t
 */
esp_zb_zcl_color_step_saturation_cmd_t cmd_req10 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .step_mode = 0x01, // Step Up (incrementing saturation)
    .step_size = 0x40, // Step size of 64
    .transition_time = 10, // 1.0 seconds
};

/**
 * @brief esp_zb_color_move_to_hue_saturation_cmd_t
 */
esp_zb_color_move_to_hue_saturation_cmd_t cmd_req11 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .hue = hue, // Target Hue (64)
    .saturation = 0xFE, // Target Saturation (254)
    .transition_time = 20, // 2.0 seconds
};

/**
 * @brief esp_zb_zcl_color_move_to_color_cmd_t
 */
esp_zb_zcl_color_move_to_color_cmd_t cmd_req12 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .color_x = xy_coords.x, // Target X (approx 0.25)
    .color_y = xy_coords.y, // Target Y (approx 0.5)
    .transition_time = 30, // 3.0 seconds
};

/**
 * @brief esp_zb_zcl_color_move_color_cmd_t
 */
esp_zb_zcl_color_move_color_cmd_t cmd_req13 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .rate_x = 0x0100, // Rate X (256 steps/sec)
    .rate_y = 0x0100, // Rate Y (256 steps/sec)
};

/**
 * @brief esp_zb_zcl_color_step_color_cmd_t
 */
esp_zb_zcl_color_step_color_cmd_t cmd_req14 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .step_x = 0x0500, // Step X (1280 steps)
    .step_y = 0x0500, // Step Y (1280 steps)
    .transition_time = 10, // 1.0 seconds
};

/**
 * @brief esp_zb_zcl_color_stop_move_step_cmd_s
 */
esp_zb_zcl_color_stop_move_step_cmd_t cmd_req15 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
};

/**
 * @brief esp_zb_zcl_color_move_to_color_temperature_cmd_t
 */
esp_zb_zcl_color_move_to_color_temperature_cmd_t cmd_req16 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .color_temperature = 0x015E, // 350 Mired (approx 2857K)
    .transition_time = 25, // 2.5 seconds
};

/**
 * @brief esp_zb_zcl_color_enhanced_move_to_hue_cmd_t
 */
esp_zb_zcl_color_enhanced_move_to_hue_cmd_t cmd_req17 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .enhanced_hue = 0x4000, // Target Enhanced Hue (16384 out of 65535)
    .direction = 0x00, // Shortest Distance
    .transition_time = 10, // 1.0 seconds
};

/**
 * @brief esp_zb_zcl_color_enhanced_move_hue_cmd_t
 */
esp_zb_zcl_color_enhanced_move_hue_cmd_t cmd_req18 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .move_mode = 0x01, // Move Up
    .rate = 0x0050, // Rate (80 steps/sec)
};

/**
 * @brief esp_zb_zcl_color_enhanced_step_hue_cmd_t
 */
esp_zb_zcl_color_enhanced_step_hue_cmd_t cmd_req19 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .step_mode = 0x01, // Step Up
    .step_size = 0x0100, // Step size (256 steps)
    .transition_time = 10, // 1.0 seconds
};

/**
 * @brief esp_zb_zcl_color_enhanced_move_to_hue_saturation_cmd_t
 */
esp_zb_zcl_color_enhanced_move_to_hue_saturation_cmd_t cmd_req20 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .enhanced_hue = 0x7FFF, // Enhanced Hue (Max Saturation)
    .saturation = 0x80, // Saturation (50%)
    .transition_time = 20, // 2.0 seconds
};

/**
 * @brief esp_zb_zcl_color_color_loop_set_cmd_t
 */
esp_zb_zcl_color_color_loop_set_cmd_t cmd_req21 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .update_flags = 0x07, // Update Action, Direction, and Time
    .action = 0x01, // Start loop
    .direction = 0x00, // Incrementing hue
    .time = 0x003C, // 60 seconds for a full loop
    .start_hue = 0x1000, // Start Hue (4096 out of 65535)
};

/**
 * @brief esp_zb_zcl_color_move_color_temperature_cmd_t
 */
esp_zb_zcl_color_move_color_temperature_cmd_t cmd_req22 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .move_mode = 0x01, // Move Up (increasing color temperature/decreasing mired)
    .rate = 0x000A, // 10 units/second
    .color_temperature_minimum = 0x00C8, // 200 Mired (5000K)
    .color_temperature_maximum = 0x0190, // 400 Mired (2500K)
};

/**
 * @brief esp_zb_zcl_color_step_color_temperature_cmd_t
 */
esp_zb_zcl_color_step_color_temperature_cmd_t cmd_req23 = {
    .zcl_basic_cmd = {
        .dst_addr_u = {
            .addr_short = short_addr,
        },
        .dst_endpoint = endpoint,
        .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
    }, 
    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
    .move_mode = 0x01, // Step Up
    .step_size = 0x0010, // Step size of 16
    .transition_time = 10, // 1.0 seconds
    .color_temperature_minimum = 0x00C8, // 200 Mired (5000K)
    .color_temperature_maximum = 0x0190, // 400 Mired (2500K)
};



/* ZCL color control cluster list command */

/**
 * @brief   Send color move to hue command
 *
 * @param[in]  cmd_req  pointer to the move to hue command @ref esp_zb_zcl_color_move_to_hue_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_move_to_hue_cmd_req(&cmd_req5);

/**
 * @brief   Send color move hue command
 *
 * @param[in]  &cmd_req  pointer to the move hue command @ref esp_zb_zcl_color_move_hue_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_move_hue_cmd_req(&cmd_req6);

/**
 * @brief   Send color step hue command
 *
 * @param[in]  cmd_req  pointer to the step hue command @ref esp_zb_zcl_color_step_hue_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_step_hue_cmd_req(&cmd_req7);

/**
 * @brief   Send color move to saturation command
 *
 * @param[in]  cmd_req  pointer to the move to saturation command @ref esp_zb_zcl_color_move_to_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_move_to_saturation_cmd_req(&cmd_req8);

/**
 * @brief   Send color move saturation command
 *
 * @param[in]  cmd_req  pointer to the move saturation command @ref esp_zb_zcl_color_move_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_move_saturation_cmd_req( &cmd_req9);

/**
 * @brief   Send color step saturation command
 *
 * @param[in]  cmd_req  pointer to the step saturation command @ref esp_zb_zcl_color_step_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_step_saturation_cmd_req(&cmd_req10);

/**
 * @brief   Send color move to hue and saturation command
 *
 * @param[in]  cmd_req  pointer to the move to hue and saturation command @ref esp_zb_color_move_to_hue_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_move_to_hue_and_saturation_cmd_req(&cmd_req11);

/**
 * @brief   Send color move to color command
 *
 * @param[in]  cmd_req  pointer to the move to color command @ref esp_zb_zcl_color_move_to_color_cmd_s
 *
 * @return The transaction sequence number
 */
 uint8_t tsn = esp_zb_zcl_color_move_to_color_cmd_req(&cmd_req12);

/**
 * @brief   Send color move color command
 *
 * @param[in]  cmd_req  pointer to the move color command @ref esp_zb_zcl_color_move_color_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_move_color_cmd_req(&cmd_req13);

/**
 * @brief   Send color step color command
 *
 * @param[in]  cmd_req  pointer to the step color command @ref esp_zb_zcl_color_step_color_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_step_color_cmd_req(&cmd_req14);

/**
 * @brief   Send color stop color command
 *
 * @param[in]  cmd_req  pointer to the stop color command @ref esp_zb_zcl_color_stop_move_step_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_stop_move_step_cmd_req(&cmd_req15);

/**
 * @brief   Send color control move to color temperature command(0x0a)
 *
 * @param[in]  cmd_req  pointer to the move to color temperature command @ref esp_zb_zcl_color_move_to_color_temperature_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_move_to_color_temperature_cmd_req(&cmd_req16);

/**
 * @brief   Send color control enhanced move to hue command(0x40)
 *
 * @param[in]  cmd_req  pointer to the enhanced move to hue command @ref esp_zb_zcl_color_enhanced_move_to_hue_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_enhanced_move_to_hue_cmd_req(&cmd_req17);

/**
 * @brief   Send color control enhanced move hue command(0x41)
 *
 * @param[in]  cmd_req  pointer to the enhanced move hue command @ref esp_zb_zcl_color_enhanced_move_hue_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_enhanced_move_hue_cmd_req(&cmd_req18);

/**
 * @brief   Send color control enhanced step hue command(0x42)
 *
 * @param[in]  cmd_req  pointer to the enhanced step hue command @ref esp_zb_zcl_color_enhanced_step_hue_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_enhanced_step_hue_cmd_req( &cmd_req19);

/**
 * @brief   Send color control move to hue and saturation command(0x43)
 *
 * @param[in]  cmd_req  pointer to the enhanced move to hue saturation command @ref esp_zb_zcl_color_enhanced_move_to_hue_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_enhanced_move_to_hue_saturation_cmd_req( &cmd_req20);

/**
 * @brief   Send color control color loop set command(0x44)
 *
 * @param[in]  cmd_req  pointer to the color loop set command @ref esp_zb_zcl_color_color_loop_set_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_color_loop_set_cmd_req(&cmd_req21);

/**
 * @brief   Send color control move color temperature command(0x4b)
 *
 * @param[in]  cmd_req  pointer to the move color temperature command @ref esp_zb_zcl_color_move_color_temperature_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_move_color_temperature_cmd_req(&cmd_req22);

/**
 * @brief   Send color control step color temperature command(0x4c)
 *
 * @param[in]  cmd_req  pointer to the step color temperature command @ref esp_zb_zcl_color_step_color_temperature_cmd_s
 *
 * @return The transaction sequence number
 */
//  esp_zb_zcl_color_step_color_temperature_cmd_req( &cmd_req23);


    if (tsn > 0) {
        return true;
    }

    return false;
}


#define ZB_ZCL_CLUSTER_ID_COLOR_CONTROL           0x0300
#define ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID   0x0008
#define ZB_ZCL_ATTR_TYPE_U8                       0x10 // Standard ZCL type for uint8_t
#define ZB_ZCL_COLOR_MODE_HUE_SATURATION          0x00 // Target Value: 0x00 for Hue/Saturation




// This is the actual value we want to write: 0x00 (Hue/Saturation Mode)
uint8_t new_color_mode_value = ZB_ZCL_COLOR_MODE_HUE_SATURATION; 




// uint8_t set_hue_saturation_mode(uint8_t endpoint, uint16_t target_addr, uint16_t manuf_code, uint16_t short_addr)
// {
//     // The Write Attribute command payload:
//     // Attribute ID (2 bytes) | Attribute Type (1 byte) | Attribute Value (1 byte)
//     // 0x0008                 | 0x10 (uint8)            | 0x00
    
//     // NOTE: The exact function signature for ZCL Write Attribute is highly 
//     // dependent on your specific ESP-IDF Zigbee SDK. This is conceptual.
//     // The structure describing the single attribute to be written
// esp_zb_zcl_attribute_t color_mode_attr = {
//     .id = ZCL_BASIC_MANUFACTURER_NAME_ATTRIBUTE_ID,
//     .data = {
//         .type = ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
//         .size = MANUFACTURER_NAME_MAX_SIZE, // Corresponds to data_size
//         .value = manufacturer_name_str        // Corresponds to data_ptr
//     };
// };
   
//     esp_zb_zcl_write_attr_cmd_t cmd_cd={
//     .zcl_basic_cmd = {
//         .dst_addr_u = {
//             .addr_short = short_addr,
//         },
//         .dst_endpoint = endpoint,
//         .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
//     }, 
//     .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 
//      .manuf_specific = 0, // Not manufacturer specific
//         .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SERVER,     // Client (us) -> Server (light)
//         .dis_default_resp = 0,                               // IMPORTANT: Set to 0 to request the status response!
        
//         .manuf_code = 0x0000,
//         .attr_number = 1,                                    // Writing only 1 attribute
//         .attr_field = &color_mode_attr,  
//     };
//     // Example SDK call:
//     return esp_zb_zcl_write_attr_cmd_req(
//         esp_zb_zcl_write_attr_cmd_t
//     );
// }

// --- Verification Step: Read the attribute back ---

/**
 * @brief Utility function to read the Color Mode attribute back.
 */
// zb_ret_t read_color_mode(zb_uint8_t endpoint, zb_uint16_t target_addr)
// {
//     zb_uint8_t attr_list[2];
    
//     // 1. Attribute ID (0x0008)
//     attr_list[0] = (ZB_ZCL_COLOR_MODE_ATTRIBUTE_ID & 0xFF);
//     attr_list[1] = (ZB_ZCL_COLOR_MODE_ATTRIBUTE_ID >> 8);
    
//     // Example SDK call for Read Attribute:
//     return esp_zb_zcl_read_attr_cmd_req(
//         endpoint,                  // Src endpoint
//         target_addr,               // Dst short address
//         ZB_ZCL_COLOR_CLUSTER_ID,   // Cluster ID
//         attr_list,                 // List of attributes to read
//         1                          // Number of attributes
//     );
// }
