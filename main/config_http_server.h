#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef esp_err_t (*config_http_reload_fn_t)(void *user_data);
typedef esp_err_t (*config_http_notify_fn_t)(void *user_data);

esp_err_t config_http_server_start(config_http_reload_fn_t reload_fn,
                                   void *reload_user_data,
                                   config_http_notify_fn_t notify_ui_fn,
                                   void *notify_ui_user_data);
esp_err_t config_http_server_stop(void);
const char *config_http_server_ap_ssid(void);
const char *config_http_server_ap_password(void);
const char *config_http_server_base_url(void);
const char *config_http_server_display_address(void);
bool config_http_server_uses_sta(void);
