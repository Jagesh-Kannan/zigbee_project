#ifndef ZB_DEVICES_H
#define ZB_DEVICES_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_DEVICES 32

typedef struct {
    bool used;
    uint16_t short_addr;
    uint8_t ieee[8];
    uint8_t num_endpoints;
    uint8_t endpoints[8];
    char name[32];
} zb_device_t;

void zb_devices_init(void);
void toggle_end_device(uint16_t);
char* send_device_info_req(uint16_t);
bool set_zb_device_level_handler(uint16_t, uint8_t);
bool esp_zb_util_set_hue_value_with_on_off(uint16_t, uint8_t);
zb_device_t *zb_device_add(uint16_t short_addr, const uint8_t ieee[8], uint8_t *eps, uint8_t num_eps);
zb_device_t *zb_device_find_by_short(uint16_t short_addr);

#endif