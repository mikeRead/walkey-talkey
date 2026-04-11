#include "ui_status.h"

#include "bsp/esp-bsp.h"
#include "lvgl.h"

#include <stdlib.h>

#include "mouse_mode.h"

#define UI_BG_IDLE_COLOR 0x0B0D10
#define UI_BG_TOUCH_DOWN_COLOR 0x1E1012
#define UI_BG_ACTIVE_COLOR 0x0D1A12
#define UI_BG_BOOT_COLOR 0x111726
#define UI_CARD_IDLE_COLOR 0x171B20
#define UI_CARD_TOUCH_COLOR 0x2A1619
#define UI_CARD_ACTIVE_COLOR 0x15241A
#define UI_CARD_BOOT_COLOR 0x1A2233
#define UI_CARD_BORDER_IDLE 0x2A3138
#define UI_CARD_BORDER_TOUCH 0x8A3D46
#define UI_CARD_BORDER_ACTIVE 0x39FF88
#define UI_CARD_BORDER_BOOT 0x68B0FF
#define UI_TEXT_PRIMARY 0xF3F5F7
#define UI_TEXT_SECONDARY 0x8E9AA7
#define UI_TEXT_ACCENT 0xAEEEC2
#define UI_TEXT_BOOT 0xB8D8FF
#define UI_LIGHT_IDLE_COLOR 0x4A525B
#define UI_LIGHT_TOUCH_COLOR 0xC84B5A
#define UI_LIGHT_ACTIVE_COLOR 0x39FF88
#define UI_LIGHT_BOOT_COLOR 0x68B0FF
#define UI_BG_MULTITOUCH_COLOR 0x1A0D26
#define UI_CARD_MULTITOUCH_COLOR 0x2A1640
#define UI_CARD_BORDER_MULTITOUCH 0xAA66FF
#define UI_BOOT_MARKER_BORDER_COLOR 0x000000
#define UI_BOOT_MARKER_SIZE 52
#define UI_BOOT_MARKER_X 420
#define UI_BOOT_MARKER_Y 318

static lv_obj_t *s_touch_overlay = NULL;
static lv_obj_t *s_main_card = NULL;
static lv_obj_t *s_top_status_label = NULL;
static lv_obj_t *s_mode_label = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_swipe_indicator_label = NULL;
static lv_obj_t *s_boot_button_marker = NULL;
static lv_obj_t *s_boot_overlay = NULL;
static lv_obj_t *s_boot_title_label = NULL;
static lv_obj_t *s_boot_subtitle_label = NULL;
static lv_obj_t *s_boot_legend_label = NULL;
static ui_status_touch_event_cb_t s_touch_callback = NULL;
static void *s_touch_event_user_data = NULL;
static bool s_touch_pending = false;
static bool s_boot_overlay_visible = false;
static bool s_primary_active = false;
static bool s_touch_swipe_reported = false;
static bool s_touch_hold_reported = false;
static bool s_mouse_mode_active = false;
static bool s_multitouch_active = false;
static uint16_t s_swipe_min_distance = 40;
static lv_point_t s_touch_press_point = {0};
static lv_point_t s_touch_last_point = {0};

static void ui_status_touch_event_cb(lv_event_t *event);

static const char *ui_status_safe_text(const char *text)
{
    return (text != NULL) ? text : "";
}

static void ui_status_forward_touch_event(ui_status_touch_raw_event_t event)
{
    if (s_touch_callback != NULL) {
        s_touch_callback(event, s_touch_event_user_data);
    }
}

static void ui_status_enable_touch_bubble(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

static void ui_status_add_touch_events(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_add_event_cb(obj, ui_status_touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, ui_status_touch_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(obj, ui_status_touch_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(obj, ui_status_touch_event_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(obj, ui_status_touch_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(obj, ui_status_touch_event_cb, LV_EVENT_PRESS_LOST, NULL);
}

static bool ui_status_read_active_point(lv_point_t *point)
{
    if (point == NULL) {
        return false;
    }

    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return false;
    }

    lv_indev_get_point(indev, point);
    return true;
}

static bool ui_status_detect_swipe_direction(ui_status_touch_raw_event_t *event)
{
    if (event == NULL) {
        return false;
    }

    int dx = s_touch_last_point.x - s_touch_press_point.x;
    int dy = s_touch_last_point.y - s_touch_press_point.y;
    int abs_dx = abs(dx);
    int abs_dy = abs(dy);

    if ((abs_dx < s_swipe_min_distance) && (abs_dy < s_swipe_min_distance)) {
        return false;
    }

    if (abs_dx >= abs_dy) {
        *event = (dx >= 0) ? UI_STATUS_TOUCH_RAW_GESTURE_RIGHT : UI_STATUS_TOUCH_RAW_GESTURE_LEFT;
    } else {
        *event = (dy >= 0) ? UI_STATUS_TOUCH_RAW_GESTURE_DOWN : UI_STATUS_TOUCH_RAW_GESTURE_UP;
    }

    return true;
}

static bool ui_status_map_lvgl_gesture(ui_status_touch_raw_event_t *event)
{
    if (event == NULL) {
        return false;
    }

    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return false;
    }

    switch (lv_indev_get_gesture_dir(indev)) {
    case LV_DIR_TOP:
        *event = UI_STATUS_TOUCH_RAW_GESTURE_UP;
        return true;
    case LV_DIR_BOTTOM:
        *event = UI_STATUS_TOUCH_RAW_GESTURE_DOWN;
        return true;
    case LV_DIR_LEFT:
        *event = UI_STATUS_TOUCH_RAW_GESTURE_LEFT;
        return true;
    case LV_DIR_RIGHT:
        *event = UI_STATUS_TOUCH_RAW_GESTURE_RIGHT;
        return true;
    default:
        break;
    }

    return false;
}

static void ui_status_refresh_visual_state(void)
{
    if ((s_status_label == NULL) || (s_main_card == NULL)) {
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    uint32_t bg_color = UI_BG_IDLE_COLOR;
    uint32_t card_color = UI_CARD_IDLE_COLOR;
    uint32_t card_border_color = UI_CARD_BORDER_IDLE;
    uint32_t accent_text_color = UI_TEXT_ACCENT;
    uint32_t boot_marker_color = UI_TEXT_ACCENT;

    if (s_multitouch_active && s_mouse_mode_active) {
        bg_color = UI_BG_MULTITOUCH_COLOR;
        card_color = UI_CARD_MULTITOUCH_COLOR;
        card_border_color = UI_CARD_BORDER_MULTITOUCH;
        accent_text_color = UI_CARD_BORDER_MULTITOUCH;
    } else if (s_boot_overlay_visible) {
        bg_color = UI_BG_BOOT_COLOR;
        card_color = UI_CARD_BOOT_COLOR;
        card_border_color = UI_CARD_BORDER_BOOT;
        accent_text_color = UI_TEXT_BOOT;
        boot_marker_color = UI_LIGHT_TOUCH_COLOR;
    } else if (s_primary_active) {
        bg_color = UI_BG_ACTIVE_COLOR;
        card_color = UI_CARD_ACTIVE_COLOR;
        card_border_color = UI_CARD_BORDER_ACTIVE;
    } else if (s_touch_pending) {
        bg_color = UI_BG_TOUCH_DOWN_COLOR;
        card_color = UI_CARD_TOUCH_COLOR;
        card_border_color = UI_CARD_BORDER_TOUCH;
    }

    lv_obj_set_style_bg_color(screen, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_color(s_main_card, lv_color_hex(card_color), 0);
    lv_obj_set_style_border_color(s_main_card, lv_color_hex(card_border_color), 0);
    lv_obj_set_style_shadow_color(s_main_card, lv_color_hex(card_border_color), 0);
    lv_obj_set_style_text_color(s_mode_label, lv_color_hex(accent_text_color), 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(UI_TEXT_PRIMARY), 0);
    if (s_swipe_indicator_label != NULL) {
        lv_obj_set_style_text_color(s_swipe_indicator_label, lv_color_hex(UI_TEXT_SECONDARY), 0);
    }
    if (s_boot_button_marker != NULL) {
        lv_obj_set_style_border_color(s_boot_button_marker, lv_color_hex(UI_BOOT_MARKER_BORDER_COLOR), 0);
        lv_obj_set_style_shadow_color(s_boot_button_marker, lv_color_hex(boot_marker_color), 0);
        lv_obj_set_style_bg_color(s_boot_button_marker, lv_color_hex(boot_marker_color), 0);
    }

    /* Force a full-screen redraw so overlay and background changes repaint the entire panel. */
    lv_obj_invalidate(screen);
    lv_obj_invalidate(s_main_card);
    if (s_boot_overlay != NULL) {
        lv_obj_invalidate(s_boot_overlay);
    }
    if (s_touch_overlay != NULL) {
        lv_obj_invalidate(s_touch_overlay);
    }
}

static void ui_status_touch_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);

    switch (code) {
    case LV_EVENT_PRESSED:
        s_touch_pending = true;
        s_touch_swipe_reported = false;
        s_touch_hold_reported = false;
        if (ui_status_read_active_point(&s_touch_press_point)) {
            s_touch_last_point = s_touch_press_point;
        }
        if (s_mouse_mode_active) {
            mouse_mode_handle_press(s_touch_press_point.x, s_touch_press_point.y);
        }
        ui_status_refresh_visual_state();
        ui_status_forward_touch_event(UI_STATUS_TOUCH_RAW_PRESSED);
        break;

    case LV_EVENT_GESTURE:
        if (!s_mouse_mode_active && !s_touch_hold_reported && !s_touch_swipe_reported) {
            ui_status_touch_raw_event_t swipe_event = UI_STATUS_TOUCH_RAW_RELEASED;
            if (ui_status_map_lvgl_gesture(&swipe_event) ||
                (ui_status_read_active_point(&s_touch_last_point) && ui_status_detect_swipe_direction(&swipe_event))) {
                s_touch_pending = false;
                s_touch_swipe_reported = true;
                ui_status_refresh_visual_state();
                ui_status_forward_touch_event(swipe_event);
                if (lv_indev_active() != NULL) {
                    lv_indev_wait_release(lv_indev_active());
                }
            }
        }
        break;

    case LV_EVENT_PRESSING:
        if (s_mouse_mode_active) {
            lv_point_t move_point = {0};
            if (ui_status_read_active_point(&move_point)) {
                mouse_mode_handle_move(move_point.x, move_point.y);
            }
        } else if (!s_touch_hold_reported && !s_touch_swipe_reported &&
                   ui_status_read_active_point(&s_touch_last_point)) {
            ui_status_touch_raw_event_t swipe_event = UI_STATUS_TOUCH_RAW_RELEASED;
            if (ui_status_detect_swipe_direction(&swipe_event)) {
                s_touch_pending = false;
                s_touch_swipe_reported = true;
                ui_status_refresh_visual_state();
                ui_status_forward_touch_event(swipe_event);
                if (lv_indev_active() != NULL) {
                    lv_indev_wait_release(lv_indev_active());
                }
            }
        }
        break;

    case LV_EVENT_LONG_PRESSED:
        if (s_touch_swipe_reported) {
            break;
        }
        s_touch_pending = false;
        s_touch_hold_reported = true;
        if (s_mouse_mode_active) {
            mouse_mode_handle_long_press();
        }
        ui_status_refresh_visual_state();
        ui_status_forward_touch_event(UI_STATUS_TOUCH_RAW_LONG_PRESSED);
        break;

    case LV_EVENT_RELEASED:
        s_touch_pending = false;
        s_touch_swipe_reported = false;
        s_touch_hold_reported = false;
        if (s_mouse_mode_active) {
            mouse_mode_handle_release();
        }
        ui_status_refresh_visual_state();
        ui_status_forward_touch_event(UI_STATUS_TOUCH_RAW_RELEASED);
        break;

    case LV_EVENT_PRESS_LOST:
        s_touch_pending = false;
        s_touch_swipe_reported = false;
        s_touch_hold_reported = false;
        if (s_mouse_mode_active) {
            mouse_mode_handle_release();
        }
        ui_status_refresh_visual_state();
        ui_status_forward_touch_event(UI_STATUS_TOUCH_RAW_RELEASED);
        break;

    default:
        break;
    }
}

void ui_status_init(ui_status_touch_event_cb_t touch_event_cb, void *user_data, uint32_t hold_delay_ms)
{
    s_touch_callback = touch_event_cb;
    s_touch_event_user_data = user_data;
    s_touch_pending = false;
    s_primary_active = false;
    s_boot_overlay_visible = false;
    s_touch_swipe_reported = false;
    s_touch_hold_reported = false;
    s_touch_press_point = (lv_point_t){0};
    s_touch_last_point = (lv_point_t){0};

    bsp_display_lock(-1);

    lv_obj_t *screen = lv_screen_active();
    lv_display_t *display = lv_display_get_default();
    int32_t screen_width = 360;
    int32_t screen_height = 360;
    if (display != NULL) {
        screen_width = lv_display_get_horizontal_resolution(display);
        screen_height = lv_display_get_vertical_resolution(display);
    }

    lv_obj_clean(screen);
    lv_obj_set_pos(screen, 0, 0);
    lv_obj_set_size(screen, screen_width, screen_height);
    lv_obj_set_style_bg_color(screen, lv_color_hex(UI_BG_IDLE_COLOR), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    s_top_status_label = lv_label_create(screen);
    lv_label_set_text(s_top_status_label, "");
    lv_obj_set_width(s_top_status_label, 320);
    lv_obj_set_style_text_align(s_top_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_top_status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_top_status_label, lv_color_hex(UI_TEXT_SECONDARY), 0);
    lv_obj_align(s_top_status_label, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_add_flag(s_top_status_label, LV_OBJ_FLAG_HIDDEN);
    ui_status_enable_touch_bubble(s_top_status_label);

    s_main_card = lv_obj_create(screen);
    lv_obj_set_size(s_main_card, 276, 196);
    lv_obj_align(s_main_card, LV_ALIGN_CENTER, 0, -4);
    lv_obj_clear_flag(s_main_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_main_card, 28, 0);
    lv_obj_set_style_border_width(s_main_card, 2, 0);
    lv_obj_set_style_pad_all(s_main_card, 0, 0);
    lv_obj_set_style_shadow_width(s_main_card, 22, 0);
    lv_obj_set_style_shadow_opa(s_main_card, LV_OPA_30, 0);
    ui_status_enable_touch_bubble(s_main_card);

    s_mode_label = lv_label_create(s_main_card);
    lv_label_set_text(s_mode_label, "");
    lv_obj_set_width(s_mode_label, 248);
    lv_obj_set_style_text_align(s_mode_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_mode_label, &lv_font_montserrat_26, 0);
    lv_obj_align(s_mode_label, LV_ALIGN_TOP_MID, 0, 18);
    ui_status_enable_touch_bubble(s_mode_label);

    s_status_label = lv_label_create(s_main_card);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_width(s_status_label, 236);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_22, 0);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 18);
    ui_status_enable_touch_bubble(s_status_label);

    s_swipe_indicator_label = lv_label_create(s_main_card);
    lv_label_set_text(s_swipe_indicator_label, "");
    lv_obj_set_width(s_swipe_indicator_label, 236);
    lv_obj_set_style_text_font(s_swipe_indicator_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_swipe_indicator_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_swipe_indicator_label, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_flag(s_swipe_indicator_label, LV_OBJ_FLAG_HIDDEN);
    ui_status_enable_touch_bubble(s_swipe_indicator_label);

    s_boot_button_marker = lv_obj_create(screen);
    lv_obj_remove_style_all(s_boot_button_marker);
    lv_obj_set_size(s_boot_button_marker, UI_BOOT_MARKER_SIZE, UI_BOOT_MARKER_SIZE);
    lv_obj_set_pos(s_boot_button_marker, UI_BOOT_MARKER_X, UI_BOOT_MARKER_Y);
    lv_obj_clear_flag(s_boot_button_marker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_boot_button_marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_boot_button_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_boot_button_marker, lv_color_hex(UI_TEXT_ACCENT), 0);
    lv_obj_set_style_bg_opa(s_boot_button_marker, LV_OPA_50, 0);
    lv_obj_set_style_border_width(s_boot_button_marker, 5, 0);
    lv_obj_set_style_border_color(s_boot_button_marker, lv_color_hex(UI_BOOT_MARKER_BORDER_COLOR), 0);
    lv_obj_set_style_shadow_width(s_boot_button_marker, 24, 0);
    lv_obj_set_style_shadow_opa(s_boot_button_marker, LV_OPA_60, 0);
    lv_obj_set_style_shadow_color(s_boot_button_marker, lv_color_hex(UI_TEXT_ACCENT), 0);

    s_boot_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(s_boot_overlay);
    lv_obj_set_pos(s_boot_overlay, 0, 0);
    lv_obj_set_size(s_boot_overlay, screen_width, screen_height);
    lv_obj_add_flag(s_boot_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_boot_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_boot_overlay, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_boot_overlay, LV_OBJ_FLAG_HIDDEN);
    ui_status_enable_touch_bubble(s_boot_overlay);
    ui_status_add_touch_events(s_boot_overlay);

    s_boot_title_label = lv_label_create(s_boot_overlay);
    lv_label_set_text(s_boot_title_label, "Swipe to switch mode");
    lv_obj_set_style_text_font(s_boot_title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(s_boot_title_label, lv_color_hex(UI_TEXT_PRIMARY), 0);
    lv_obj_align(s_boot_title_label, LV_ALIGN_TOP_MID, 0, 22);
    ui_status_enable_touch_bubble(s_boot_title_label);

    s_boot_subtitle_label = lv_label_create(s_boot_overlay);
    lv_obj_set_width(s_boot_subtitle_label, 280);
    lv_label_set_text(s_boot_subtitle_label, "");
    lv_obj_set_style_text_align(s_boot_subtitle_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_boot_subtitle_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_boot_subtitle_label, lv_color_hex(UI_TEXT_BOOT), 0);
    lv_obj_align(s_boot_subtitle_label, LV_ALIGN_TOP_MID, 0, 50);
    ui_status_enable_touch_bubble(s_boot_subtitle_label);

    s_boot_legend_label = lv_label_create(s_boot_overlay);
    lv_obj_set_width(s_boot_legend_label, 240);
    lv_label_set_text(s_boot_legend_label, "Release BOOT = Confirm");
    lv_obj_set_style_text_align(s_boot_legend_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_boot_legend_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_boot_legend_label, lv_color_hex(UI_TEXT_PRIMARY), 0);
    lv_obj_align(s_boot_legend_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    ui_status_enable_touch_bubble(s_boot_legend_label);

    s_touch_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(s_touch_overlay);
    lv_obj_set_pos(s_touch_overlay, 0, 0);
    lv_obj_set_size(s_touch_overlay, screen_width, screen_height);
    lv_obj_add_flag(s_touch_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_touch_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_touch_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_touch_overlay, 0, 0);
    ui_status_add_touch_events(s_touch_overlay);
    lv_obj_move_foreground(s_touch_overlay);
    lv_obj_move_foreground(s_boot_button_marker);

    if (bsp_display_get_input_dev() != NULL) {
        lv_indev_set_long_press_time(bsp_display_get_input_dev(), hold_delay_ms);
    }

    ui_status_refresh_visual_state();
    lv_obj_invalidate(screen);
    bsp_display_unlock();
}

void ui_status_set_swipe_min_distance(uint16_t swipe_min_distance)
{
    s_swipe_min_distance = swipe_min_distance;
}

void ui_status_set_mouse_mode(bool enabled)
{
    s_mouse_mode_active = enabled;
}

void ui_status_set_multitouch_indicator(bool active)
{
    if (s_multitouch_active == active) {
        return;
    }
    s_multitouch_active = active;
    ui_status_refresh_visual_state();
}

void ui_status_render(const ui_status_view_model_t *view_model)
{
    if (view_model == NULL) {
        return;
    }

    bsp_display_lock(-1);

    s_primary_active = view_model->primary_active;
    s_boot_overlay_visible = view_model->boot_overlay_visible;

    if (s_top_status_label != NULL) {
        const char *top_status_text = ui_status_safe_text(view_model->top_status_text);
        lv_label_set_text(s_top_status_label, top_status_text);
        if ((top_status_text[0] != '\0') && !view_model->boot_overlay_visible) {
            lv_obj_clear_flag(s_top_status_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_top_status_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_mode_label != NULL) {
        lv_label_set_text(s_mode_label, ui_status_safe_text(view_model->mode_label));
    }
    if (s_status_label != NULL) {
        lv_label_set_text(s_status_label, ui_status_safe_text(view_model->status_text));
    }

    if (s_boot_title_label != NULL) {
        lv_label_set_text(s_boot_title_label, ui_status_safe_text(view_model->boot_title));
    }
    if (s_boot_subtitle_label != NULL) {
        lv_label_set_text(s_boot_subtitle_label, ui_status_safe_text(view_model->boot_subtitle));
    }
    if (s_swipe_indicator_label != NULL) {
        const char *touch_debug_text = ui_status_safe_text(view_model->touch_debug_text);
        lv_label_set_text(s_swipe_indicator_label, touch_debug_text);
        if ((touch_debug_text[0] != '\0') && !view_model->boot_overlay_visible) {
            lv_obj_clear_flag(s_swipe_indicator_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_swipe_indicator_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_boot_overlay != NULL) {
        if (s_boot_overlay_visible) {
            lv_obj_clear_flag(s_boot_overlay, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_boot_overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }

    ui_status_refresh_visual_state();
    lv_obj_invalidate(lv_screen_active());
    bsp_display_unlock();
}
