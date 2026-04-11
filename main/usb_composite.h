#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "mode_types.h"

esp_err_t usb_composite_init(void);
bool usb_composite_hid_ready(void);
bool usb_composite_audio_ready(void);
void usb_composite_set_audio_capture_suspended(bool suspended);
void usb_composite_set_ptt_audio_active(bool active);
uint32_t usb_composite_audio_stack_high_water_mark(void);
esp_err_t usb_composite_send_usage(bool pressed, const mode_hid_usage_t *usage);
esp_err_t usb_composite_release_all_keys(void);
esp_err_t usb_composite_send_f13_down(void);
esp_err_t usb_composite_send_f13_up(void);
esp_err_t usb_composite_send_f14_down(void);
esp_err_t usb_composite_send_f14_up(void);
esp_err_t usb_composite_send_mouse_report(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel);
