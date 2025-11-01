#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_zigbee_gateway.h"
#include "zb_config_platform.h"
#include "zb_devices.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/semphr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const uint8_t webInterface_html_start[] asm("_binary_webInterface_html_start");
extern const uint8_t webInterface_html_end[]   asm("_binary_webInterface_html_end");

#define MAX_SSE_CLIENTS 6
typedef struct {
    httpd_req_t *req;
    bool used;
} sse_client_t;

static sse_client_t sse_clients[MAX_SSE_CLIENTS];
static SemaphoreHandle_t sse_lock;


static uint16_t c_sort_addrs = 0xC12F;
static char* g_device_name = "";

static const char *TAG = "ZB_WEB";

static const char *TAGd = "ESP_ZB_GATEWAY";

static httpd_handle_t server = NULL;

/* Small helper to produce JSON status */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    char buf[256];
    uint16_t pan_id = esp_zb_get_pan_id();
    uint8_t channel = esp_zb_get_current_channel();
    uint16_t short_addr = esp_zb_get_short_address();

    int len = snprintf(buf, sizeof(buf),
        "{ \"pan_id\": \"0x%04x\", \"channel\": %u, \"short_addr\": \"0x%04x\" }",
        pan_id, channel, short_addr);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, len);
    return ESP_OK;
}

/* Open network for joining (seconds parameter optional via query ?t=seconds) */
static esp_err_t permit_put_handler(httpd_req_t *req)
{
    int permit_time = 180; /* default */

    /* get query string length and content */
    int qlen = httpd_req_get_url_query_len(req);
    if (qlen > 0) {
        char *query = calloc(qlen + 1, 1);
        if (query) {
            if (httpd_req_get_url_query_str(req, query, qlen + 1) == ESP_OK) {
                char val[12];
                if (httpd_query_key_value(query, "t", val, sizeof(val)) == ESP_OK) {
                    int v = atoi(val);
                    if (v > 0) permit_time = v;
                }
            }
            free(query);
        }
    }

    esp_err_t ret = esp_zb_bdb_open_network((uint8_t)permit_time);
    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"result\":\"ok\"}");
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"result\":\"error\"}");
    }
    return ESP_OK;
}


static esp_err_t sse_get_handler(httpd_req_t *req)
{

    //  sse_lock = xSemaphoreCreateMutex();

      if (sse_lock == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }


    /* set headers required for SSE */
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");

    /* register client */
    if (xSemaphoreTake(sse_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
        int slot = -1;
        for (int i = 0; i < MAX_SSE_CLIENTS; ++i) {
            if (!sse_clients[i].used) { slot = i; break; }
        }
        if (slot >= 0) {
            sse_clients[slot].used = true;
            sse_clients[slot].req = req;
            /* send an initial comment or ping so client knows connection is open */
            const char *init = ": connected\n\n";
            httpd_resp_send_chunk(req, init, strlen(init));
            xSemaphoreGive(sse_lock);

            /* DO NOT call httpd_resp_send or return a final value: keep connection open */
            /* Wait until client closes connection (httpd will block until client sends data or disconnects) */
            while (sse_clients[slot].used) {
                /* sleep a bit to yield CPU; real implementation could use notifications to end connection */
                vTaskDelay(pdMS_TO_TICKS(1000));
                /* test connection by sending a single line ping every 30s to keep alive */
                /* optional: implement keepalive/ping here */
                /* if client closed, httpd_resp_send_chunk will fail in sse_send_to_all and free slot */
                 httpd_resp_send_chunk(req, NULL, 0);
                if (!sse_clients[slot].used) break;
            }
            return ESP_OK;
        }
        xSemaphoreGive(sse_lock);
    }

    /* no slot available */
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

/* Close network (permit 0 seconds) */
static esp_err_t permit_delete_handler(httpd_req_t *req)
{
    esp_err_t ret = esp_zb_bdb_open_network(0);
    if (ret == ESP_OK) {
        httpd_resp_sendstr(req, "{\"result\":\"ok\"}");
    } else {
        httpd_resp_sendstr(req, "{\"result\":\"error\"}");
    }
    return ESP_OK;
}

/*  toogle the light on and off */
static esp_err_t endDevToggle(httpd_req_t *req)
{
    
   uint16_t sort_addr = 0xC12F;
   
  
  ESP_LOGI(TAGd, "LOOP 0 %s","initiali");
    int qlen = httpd_req_get_url_query_len(req);
    if (qlen > 0) {
        char *query = calloc(qlen + 1, 1);
        if (query) {
                ESP_LOGI(TAGd, "LOOP 1 %s",query);
            if (httpd_req_get_url_query_str(req, query, qlen + 1) == ESP_OK) {
                ESP_LOGI(TAGd, "LOOP 2 %s",query);
                char val[12];
                if (httpd_query_key_value(query, "sad", val, sizeof(val)) == ESP_OK) {
                    ESP_LOGI(TAGd, "LOOP 3 %s",val);

                    char *endptr;
                   unsigned long v_ul = strtoul(val, &endptr, 16);
            
          
                    if (*endptr == '\0' && v_ul <= 0xFFFFUL) { // 0xFFFFUL is 65535
                        sort_addr = (uint16_t)v_ul;
                        ESP_LOGI(TAGd, "Successfully converted Short Address: 0x%04hx", sort_addr);
                    } else {
                        ESP_LOGE(TAGd, "Failed to convert SAD value '%s' or value is out of range.", val);
                        // Handle error (e.g., return a 400 Bad Request response)
                    }
                    ESP_LOGI(TAGd, "LOOP 4 %d",sort_addr);
                }
            }
             ESP_LOGI(TAGd, "LOG 5 0x%04hx", sort_addr);
            free(query);
        }
    }

 ESP_LOGI(TAGd, "LOG 6 0x%04hx", sort_addr);
    
    toggle_end_device(sort_addr);

    httpd_resp_sendstr(req, "{\"result\":\"ok\"}");
    return ESP_OK;
}


static esp_err_t set_device_level_handler(httpd_req_t *req){

    char *buf = NULL;
    size_t buf_len = 0;
    char sad_str[5] = {0}; // 4 hex digits + NULL
    char level_str[4] = {0}; // Max 254 (3 digits) + NULL
    
    // Get query string length
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            
            // 1. Parse Short Address (sad)
            if (httpd_query_key_value(buf, "sad", sad_str, sizeof(sad_str)) != ESP_OK) {
                ESP_LOGE(TAG, "Missing sad parameter");
                goto _400_bad_request;
            }
            
            // 2. Parse Level Value (level)
            if (httpd_query_key_value(buf, "level", level_str, sizeof(level_str)) != ESP_OK) {
                ESP_LOGE(TAG, "Missing level parameter");
                goto _400_bad_request;
            }

            // 3. Conversion: Short Address (Hex to u16), Level (Dec to u8)
            uint16_t short_addr = (uint16_t)strtol(sad_str, NULL, 16); 
            uint8_t level = (uint8_t)strtol(level_str, NULL, 10);   
            
            if( set_zb_device_level_handler( short_addr, level)){

                 httpd_resp_send(req, "Level command sent", HTTPD_RESP_USE_STRLEN);
            }else{
                 ESP_LOGE(TAG, "Failed to send Level Control command");
                  httpd_resp_send_500(req);
            }
            free(buf);
            return ESP_OK;
        }
    }
    
_400_bad_request:
    if (buf) free(buf);
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t set_device_hue_handler(httpd_req_t *req){

    char *buf = NULL;
    size_t buf_len = 0;
    char sad_str[5] = {0}; // 4 hex digits + NULL
    char hue_str[4] = {0}; // Max 254 (3 digits) + NULL
    
    // Get query string length
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            
            // 1. Parse Short Address (sad)
            if (httpd_query_key_value(buf, "sad", sad_str, sizeof(sad_str)) != ESP_OK) {
                ESP_LOGE(TAG, "Missing sad parameter");
                goto _400_bad_request;
            }
            
            // 2. Parse Level Value (level)
            if (httpd_query_key_value(buf, "hue", hue_str, sizeof(hue_str)) != ESP_OK) {
                ESP_LOGE(TAG, "Missing level parameter");
                goto _400_bad_request;
            }

            // 3. Conversion: Short Address (Hex to u16), Level (Dec to u8)
            uint16_t short_addr = (uint16_t)strtol(sad_str, NULL, 16); 
            uint8_t hue = (uint8_t)strtol(hue_str, NULL, 10);   
            
            if( esp_zb_util_set_hue_value_with_on_off( short_addr, hue)){

                 httpd_resp_send(req, "Level command sent", HTTPD_RESP_USE_STRLEN);
            }else{
                 ESP_LOGE(TAG, "Failed to send Level Control command");
                  httpd_resp_send_500(req);
            }
            free(buf);
            return ESP_OK;
        }
    }
    
_400_bad_request:
    if (buf) free(buf);
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t device_state(httpd_req_t *req){

      char buf[256];
    uint16_t short_addr = c_sort_addrs;

    int len = snprintf(buf, sizeof(buf),
        "{ \"type\": \"device_paired\", \"short_addr\": \"0x%04x\", \"deviceName\":\"%s\" }",
         short_addr, g_device_name);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, len);
    return ESP_OK;
}
/* Serve the single-page UI */
// static const char INDEX_HTML[] =
// "<!doctype html>"
// "<html><head><meta charset='utf-8'><title>Zigbee Gateway</title>"
// "<style>body{font-family:Arial;margin:20px;}button{margin-right:8px;padding:8px 12px;}</style>"
// "</head><body>"
// "<h2>Zigbee Gateway</h2>"
// "<div id='status'>Loading status...</div>"
// "<p>"
// "<button id='open'>Open network (180s)</button>"
// "<button id='close'>Close network</button>"
// "<div id='deviceName'>Loading... -> </div>"
// "<button id='toggle' name=sort_addr>ON/OFF</button>"
// "</p>"
// "<script>"
//  "let sort_addr ;"
// "async function getStatus(){"
// "  try{const r=await fetch('/api/status'); const j=await r.json();"
// "  document.getElementById('status').innerText = 'PAN: '+j.pan_id+'  Channel: '+j.channel+'  Short: '+j.short_addr; }catch(e){document.getElementById('status').innerText='Error fetching status';}}"
// "document.getElementById('open').addEventListener('click', async ()=>{"
// "  await fetch('/api/permit?t=180',{method:'PUT'}); getStatus();});"
// "document.getElementById('close').addEventListener('click', async ()=>{"
// "  await fetch('/api/permit',{method:'DELETE'}); getStatus();});"
// "getStatus(); setInterval(getStatus,5000);"
// "document.getElementById('toggle').addEventListener('click', async ()=>{"
// "  await fetch('/api/endDeviceLightToggle?sad='+sort_addr,{method:'GET'}); });"


// "const eventLog = document.createElement('div');"
// "document.body.appendChild(eventLog); "


// "async function getEvent(){"

//     "try {"
//     "  const r=await fetch('/api/events',{method:'GET'}); const j=await r.json();"
//     "console.log('Received data:', j);"
//         "const data = j;"
//         "if (data.type === 'device_paired') {"
//            "document.getElementById('pairedmessage')?.remove();"
//            " const message = document.createElement('p');"
//            " const device_name = document.getElementById('deviceName');"
//            "device_name.innerHTML = `${data.deviceName}`;"
//             "message.id='pairedmessage';"
//             "message.innerHTML = `✅ **NEW DEVICE PAIRED:** Short Address: **${data.short_addr}**`;"
//             "eventLog.prepend(message); "
//             "sort_addr = data.short_addr;"
//        " };"

//     "} catch (e) {"
//         "console.error('Error parsing JSON data:', e);"
//     "}"
// "};"

// "setInterval(getEvent,5000);"

// "</script>"
// "</body></html>";

static esp_err_t index_get_handler(httpd_req_t *req)
{

    const size_t webInterface_html_len = webInterface_html_end - webInterface_html_start;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)webInterface_html_start, webInterface_html_len);
    return ESP_OK;
}


static void sse_init(void)
{
    sse_lock = xSemaphoreCreateMutex();
    for (int i = 0; i < MAX_SSE_CLIENTS; ++i) sse_clients[i].used = false;
}

static void sse_send_to_all(const char *data)
{
    if (!sse_lock) return;
    if (xSemaphoreTake(sse_lock, pdMS_TO_TICKS(200)) != pdTRUE) return;
    for (int i = 0; i < MAX_SSE_CLIENTS; ++i) {
        if (!sse_clients[i].used || !sse_clients[i].req) continue;
        /* Send one event: "data: <json>\n\n" using chunked transfer */
        char evbuf[512];
        int evlen = snprintf(evbuf, sizeof(evbuf), "data: %s\n\n", data);
        if (evlen > 0 && evlen < (int)sizeof(evbuf)) {
            /* send chunk (non-blocking on client disconnects) */
            if (httpd_resp_send_chunk(sse_clients[i].req, evbuf, evlen) != ESP_OK) {
                /* client closed or error -> remove it */
                httpd_resp_send_chunk(sse_clients[i].req, NULL, 0);
                sse_clients[i].used = false;
                sse_clients[i].req = NULL;
            }
        }
    }
    xSemaphoreGive(sse_lock);
}


esp_err_t web_server_start(void)
{

    sse_init();
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    config.max_open_sockets = 7;

    config.task_priority = configMAX_PRIORITIES - 3; // Set to a high priority (e.g., 7-9 depending on your max)
    config.stack_size    = 8192; // Increase stack from default (e.g., 4096) for safety
    
    // Optional: Setting a short idle timeout helps close dangling connections quicker
    config.recv_wait_timeout = 5; // seconds
    config.send_wait_timeout = 5; // seconds

    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    /* Register routes */
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &status_uri);

    httpd_uri_t permit_put_uri = {
        .uri = "/api/permit",
        .method = HTTP_PUT,
        .handler = permit_put_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &permit_put_uri);

    httpd_uri_t permit_del_uri = {
        .uri = "/api/permit",
        .method = HTTP_DELETE,
        .handler = permit_delete_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &permit_del_uri);

    
    // -------   toggle on / off -----------------
        httpd_uri_t toggle_uri = {
        .uri = "/api/endDeviceLightToggle",
        .method = HTTP_GET,
        .handler = endDevToggle,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &toggle_uri);


   //------------- Level Control (DIMMING) ------------------
    httpd_uri_t level_put_uri = {
        .uri = "/api/setDeviceLevel",
        .method = HTTP_PUT,
        .handler = set_device_level_handler, // NEW HANDLER
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &level_put_uri);


    //----- Color Control (HUE)  ------------
    httpd_uri_t hue_put_uri = {
        .uri = "/api/setDeviceHue",
        .method = HTTP_PUT,
        .handler = set_device_hue_handler, // NEW HANDLER
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &hue_put_uri);


            httpd_uri_t sse_uri = {
            .uri = "/api/events",
            .method = HTTP_GET,
            .handler = device_state,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &sse_uri);

    return ESP_OK;
}

void web_server_notify_device_paired(uint16_t short_addr, char* device_name)
{
    char data_buf[128];
    // Format the data as a JSON event payload

    c_sort_addrs = short_addr;
    g_device_name = device_name;
    int len = snprintf(data_buf, sizeof(data_buf), 
        "{\"type\":\"device_paired\", \"short_addr\":\"0x%04x\", \"deviceName\":\"%s\"}", short_addr, device_name);
    
    if (len > 0 && len < (int)sizeof(data_buf)) {
        // Pass the formatted JSON string to the internal SSE broadcast function
        sse_send_to_all(data_buf);
        ESP_LOGI(TAG, "Notified web client of new device: 0x%04x", short_addr);
    }
}


void web_server_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
}

