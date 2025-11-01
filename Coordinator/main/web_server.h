#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

void web_server_notify_device_paired(uint16_t short_addr, char*);
esp_err_t web_server_start(void);
void web_server_stop(void);

#endif // WEB_SERVER_H

