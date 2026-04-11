#include "mouse_mode.h"

#include <stdlib.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

#include "multitouch_poc.h"
#include "ui_status.h"
#include "usb_composite.h"

#define MOUSE_MODE_MOVE_THRESHOLD_PX 10
#define MOUSE_MODE_CLICK_RELEASE_MS 20
#define MOUSE_MODE_TAP_DRAG_WINDOW_MS 200
#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

static const char *TAG = "mouse_mode";

static bool s_active = false;
static lv_point_t s_last_point = {0};
static int32_t s_move_accum_x = 0;
static int32_t s_move_accum_y = 0;
static bool s_moved_significantly = false;
static bool s_drag_active = false;
static bool s_right_click_sent = false;
static bool s_multitouch_seen = false;
static uint32_t s_last_tap_release_ms = 0;

/* One-shot timer used to release the left-click button after a tap. */
static lv_timer_t *s_click_timer = NULL;

static void mouse_mode_reset_state(void)
{
    s_last_point = (lv_point_t){0};
    s_move_accum_x = 0;
    s_move_accum_y = 0;
    s_moved_significantly = false;
    s_drag_active = false;
    s_right_click_sent = false;
    s_multitouch_seen = false;
    s_last_tap_release_ms = 0;
}

static void mouse_mode_click_release_cb(lv_timer_t *timer)
{
    (void)timer;
    usb_composite_send_mouse_report(0, 0, 0, 0);
    lv_timer_pause(timer);
}

void mouse_mode_init(void)
{
    if (s_click_timer != NULL) {
        return;
    }

    mouse_mode_reset_state();

    bsp_display_lock(-1);
    s_click_timer = lv_timer_create(mouse_mode_click_release_cb, MOUSE_MODE_CLICK_RELEASE_MS, NULL);
    if (s_click_timer != NULL) {
        lv_timer_pause(s_click_timer);
    }
    bsp_display_unlock();

    ESP_LOGI(TAG, "mouse mode initialised");
}

void mouse_mode_set_active(bool active)
{
    if (s_active == active) {
        return;
    }

    s_active = active;

    if (!active) {
        usb_composite_send_mouse_report(0, 0, 0, 0);
    }
    mouse_mode_reset_state();

    ESP_LOGI(TAG, "mouse mode %s", active ? "active" : "inactive");
}

bool mouse_mode_is_active(void)
{
    return s_active;
}

void mouse_mode_handle_press(int32_t x, int32_t y)
{
    if (!s_active) {
        return;
    }

    bool tap_drag = (s_last_tap_release_ms > 0) &&
                    ((lv_tick_get() - s_last_tap_release_ms) < MOUSE_MODE_TAP_DRAG_WINDOW_MS);

    s_last_point.x = x;
    s_last_point.y = y;
    s_move_accum_x = 0;
    s_move_accum_y = 0;
    s_moved_significantly = false;
    s_drag_active = false;
    s_last_tap_release_ms = 0;

    if (tap_drag) {
        s_drag_active = true;
        esp_err_t err = usb_composite_send_mouse_report(MOUSE_BUTTON_LEFT, 0, 0, 0);
        ESP_LOGI(TAG, "tap-drag start at (%d,%d) err=%s", (int)x, (int)y, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "press at (%d,%d)", (int)x, (int)y);
    }

    if (multitouch_poc_get_touch_count() >= 2) {
        s_multitouch_seen = true;
    }
    ui_status_set_multitouch_indicator(s_multitouch_seen);
}

void mouse_mode_handle_move(int32_t x, int32_t y)
{
    if (!s_active) {
        return;
    }

    int32_t touch_dx = x - s_last_point.x;
    int32_t touch_dy = y - s_last_point.y;
    s_last_point.x = x;
    s_last_point.y = y;

    /* Compensate for 90-degree CW physical screen rotation. */
    int32_t raw_dx = -touch_dy;
    int32_t raw_dy = touch_dx;

    s_move_accum_x += touch_dx;
    s_move_accum_y += touch_dy;
    if ((abs(s_move_accum_x) + abs(s_move_accum_y)) >= MOUSE_MODE_MOVE_THRESHOLD_PX) {
        s_moved_significantly = true;
    }

    if (multitouch_poc_get_touch_count() >= 2) {
        s_multitouch_seen = true;
    }
    ui_status_set_multitouch_indicator(s_multitouch_seen);

    if ((raw_dx == 0) && (raw_dy == 0)) {
        return;
    }

    int8_t dx = (raw_dx > 127) ? 127 : (raw_dx < -128) ? -128 : (int8_t)raw_dx;
    int8_t dy = (raw_dy > 127) ? 127 : (raw_dy < -128) ? -128 : (int8_t)raw_dy;
    uint8_t buttons = s_drag_active ? MOUSE_BUTTON_LEFT : 0;
    esp_err_t err = usb_composite_send_mouse_report(buttons, dx, dy, 0);
    ESP_LOGI(TAG, "move dx=%d dy=%d drag=%d err=%s", (int)dx, (int)dy, s_drag_active, esp_err_to_name(err));
}

void mouse_mode_handle_long_press(void)
{
    if (!s_active || s_moved_significantly || s_drag_active) {
        return;
    }

    s_right_click_sent = true;
    uint8_t button = s_multitouch_seen ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_RIGHT;
    esp_err_t err = usb_composite_send_mouse_report(button, 0, 0, 0);
    if (s_click_timer != NULL) {
        lv_timer_reset(s_click_timer);
        lv_timer_resume(s_click_timer);
    }
    ESP_LOGI(TAG, "%s click err=%s",
             s_multitouch_seen ? "middle" : "right", esp_err_to_name(err));
}

void mouse_mode_handle_release(void)
{
    if (!s_active) {
        return;
    }

    if (s_drag_active) {
        esp_err_t err = usb_composite_send_mouse_report(0, 0, 0, 0);
        ESP_LOGI(TAG, "drag end (left up) err=%s", esp_err_to_name(err));
    } else if (!s_moved_significantly && !s_right_click_sent) {
        esp_err_t err = usb_composite_send_mouse_report(MOUSE_BUTTON_LEFT, 0, 0, 0);
        if (s_click_timer != NULL) {
            lv_timer_reset(s_click_timer);
            lv_timer_resume(s_click_timer);
        }
        s_last_tap_release_ms = lv_tick_get();
        ESP_LOGI(TAG, "left click err=%s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "drag end");
    }

    s_move_accum_x = 0;
    s_move_accum_y = 0;
    s_moved_significantly = false;
    s_drag_active = false;
    s_right_click_sent = false;
    s_multitouch_seen = false;

    ui_status_set_multitouch_indicator(false);
}
