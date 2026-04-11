#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef void (*boot_button_event_cb_t)(bool pressed, void *user_data);

esp_err_t boot_button_init(boot_button_event_cb_t event_cb, void *user_data);
bool boot_button_is_pressed(void);
