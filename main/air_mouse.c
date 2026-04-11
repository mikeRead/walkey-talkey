#include "air_mouse.h"

#include <math.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "qmi8658.h"
#include "usb_composite.h"

#define AIR_MOUSE_TASK_STACK_SIZE 4096
#define AIR_MOUSE_TASK_PRIORITY   5
#define AIR_MOUSE_POLL_MS         10
#define AIR_MOUSE_CALIBRATION_DELAY_MS 10
#define AIR_MOUSE_REWIND_MAX      16

typedef struct {
    int8_t dx;
    int8_t dy;
} delta_sample_t;

static air_mouse_config_t s_cfg;
static delta_sample_t s_rewind_buf[AIR_MOUSE_REWIND_MAX];
static int s_rewind_idx = 0;
static int s_rewind_count = 0;

static float s_ema_yaw   = 0.0f;
static float s_ema_pitch = 0.0f;

static const char *TAG = "air_mouse";

static qmi8658_dev_t s_imu;
static TaskHandle_t  s_task_handle = NULL;
static volatile bool s_active   = false;
static volatile bool s_tracking = false;
static volatile uint8_t s_buttons = 0;
static volatile float s_accumulated_movement = 0.0f;
static bool s_imu_ok = false;

static float s_bias_gx = 0.0f;
static float s_bias_gy = 0.0f;
static float s_bias_gz = 0.0f;

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static float apply_easing(float dps)
{
    float abs_dps = fabsf(dps);
    if (abs_dps < 0.001f) {
        return 0.0f;
    }
    float norm = abs_dps / s_cfg.max_dps;
    if (norm > 1.0f) {
        norm = 1.0f;
    }
    float curved = powf(norm, s_cfg.easing_exponent);
    float sign = (dps >= 0.0f) ? 1.0f : -1.0f;
    return sign * curved * s_cfg.max_dps * s_cfg.sensitivity;
}

static esp_err_t air_mouse_calibrate(void)
{
    float sum_gx = 0.0f, sum_gy = 0.0f, sum_gz = 0.0f;
    int good = 0;

    int cal_samples = (int)s_cfg.calibration_samples;
    ESP_LOGI(TAG, "calibrating gyro (%d samples)...", cal_samples);

    for (int i = 0; i < cal_samples; i++) {
        float gx, gy, gz;
        if (qmi8658_read_gyro(&s_imu, &gx, &gy, &gz) == ESP_OK) {
            sum_gx += gx;
            sum_gy += gy;
            sum_gz += gz;
            good++;
        }
        vTaskDelay(pdMS_TO_TICKS(AIR_MOUSE_CALIBRATION_DELAY_MS));
    }

    if (good < 10) {
        ESP_LOGW(TAG, "calibration failed -- only %d good samples", good);
        return ESP_FAIL;
    }

    s_bias_gx = sum_gx / good;
    s_bias_gy = sum_gy / good;
    s_bias_gz = sum_gz / good;

    ESP_LOGI(TAG, "gyro bias: gx=%.3f gy=%.3f gz=%.3f (%d samples)",
             s_bias_gx, s_bias_gy, s_bias_gz, good);
    return ESP_OK;
}

static void air_mouse_task(void *arg)
{
    (void)arg;

    for (;;) {
        if (!s_active) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }

        if (!s_tracking) {
            vTaskDelay(pdMS_TO_TICKS(AIR_MOUSE_POLL_MS));
            continue;
        }

        float gx, gy, gz;
        esp_err_t ret = qmi8658_read_gyro(&s_imu, &gx, &gy, &gz);
        if (ret != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(AIR_MOUSE_POLL_MS));
            continue;
        }

        gx -= s_bias_gx;
        gy -= s_bias_gy;
        gz -= s_bias_gz;

        /*
         * Axis mapping (device held face-up, USB port away from user):
         *   gz (yaw)   -> horizontal mouse dx
         *   gx (pitch) -> vertical   mouse dy
         * Signs may need flipping depending on physical orientation.
         */
        float yaw   = gz;
        float pitch = gx;

        s_accumulated_movement += fabsf(yaw) + fabsf(pitch);

        s_ema_yaw   = s_cfg.ema_alpha * yaw   + (1.0f - s_cfg.ema_alpha) * s_ema_yaw;
        s_ema_pitch = s_cfg.ema_alpha * pitch  + (1.0f - s_cfg.ema_alpha) * s_ema_pitch;
        yaw   = s_ema_yaw;
        pitch = s_ema_pitch;

        if (fabsf(yaw) < s_cfg.dead_zone_dps) {
            yaw = 0.0f;
        }
        if (fabsf(pitch) < s_cfg.dead_zone_dps) {
            pitch = 0.0f;
        }

        int8_t dx = (int8_t)clampf(apply_easing(yaw),   -127.0f, 127.0f);
        int8_t dy = (int8_t)clampf(apply_easing(pitch), -127.0f, 127.0f);

        int rd = (int)s_cfg.rewind_depth;
        s_rewind_buf[s_rewind_idx].dx = dx;
        s_rewind_buf[s_rewind_idx].dy = dy;
        s_rewind_idx = (s_rewind_idx + 1) % rd;
        if (s_rewind_count < rd) {
            s_rewind_count++;
        }

        if ((dx != 0) || (dy != 0) || (s_buttons != 0)) {
            usb_composite_send_mouse_report(s_buttons, dx, dy, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(AIR_MOUSE_POLL_MS));
    }
}

esp_err_t air_mouse_init(const air_mouse_config_t *cfg)
{
    if (s_task_handle != NULL) {
        return ESP_OK;
    }

    if (cfg != NULL) {
        s_cfg = *cfg;
    }
    if (s_cfg.rewind_depth > AIR_MOUSE_REWIND_MAX) {
        s_cfg.rewind_depth = AIR_MOUSE_REWIND_MAX;
    }
    if (s_cfg.rewind_depth == 0) {
        s_cfg.rewind_depth = 1;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not available");
        return ESP_ERR_INVALID_STATE;
    }

    /* Try high address first, then low. */
    esp_err_t ret = qmi8658_init(&s_imu, bus, QMI8658_ADDRESS_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 not at 0x%02X, trying 0x%02X",
                 QMI8658_ADDRESS_HIGH, QMI8658_ADDRESS_LOW);
        if (s_imu.dev_handle != NULL) {
            i2c_master_bus_rm_device(s_imu.dev_handle);
        }
        ret = qmi8658_init(&s_imu, bus, QMI8658_ADDRESS_LOW);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 init failed: %s", esp_err_to_name(ret));
        if (s_imu.dev_handle != NULL) {
            i2c_master_bus_rm_device(s_imu.dev_handle);
        }
        return ret;
    }
    s_imu_ok = true;

    air_mouse_calibrate();

    BaseType_t ok = xTaskCreate(air_mouse_task, "air_mouse",
                                AIR_MOUSE_TASK_STACK_SIZE, NULL,
                                AIR_MOUSE_TASK_PRIORITY, &s_task_handle);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create air_mouse task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "air mouse initialised");
    return ESP_OK;
}

void air_mouse_set_active(bool active)
{
    if (!s_imu_ok) {
        return;
    }

    if (s_active == active) {
        return;
    }

    s_active = active;
    s_tracking = false;
    s_buttons  = 0;
    s_accumulated_movement = 0.0f;

    if (active && (s_task_handle != NULL)) {
        xTaskNotifyGive(s_task_handle);
    }

    ESP_LOGI(TAG, "air mouse %s", active ? "active" : "inactive");
}

bool air_mouse_is_active(void)
{
    return s_active;
}

void air_mouse_set_tracking(bool tracking)
{
    if (tracking && !s_tracking) {
        s_accumulated_movement = 0.0f;
        s_rewind_idx = 0;
        s_rewind_count = 0;
        s_ema_yaw = 0.0f;
        s_ema_pitch = 0.0f;
    }

    if (!tracking && s_tracking && s_rewind_count > 0) {
        /*
         * Weighted rewind: walk the ring buffer from newest to oldest.
         * Each step back multiplies the weight by REWIND_DECAY so recent
         * samples (most likely pure release jerk) dominate.
         */
        int rd = (int)s_cfg.rewind_depth;
        float wsum_dx = 0.0f, wsum_dy = 0.0f, total_w = 0.0f;
        float w = 1.0f;
        int idx = (s_rewind_idx - 1 + rd) % rd;
        for (int i = 0; i < s_rewind_count; i++) {
            wsum_dx += w * s_rewind_buf[idx].dx;
            wsum_dy += w * s_rewind_buf[idx].dy;
            total_w += w;
            w *= s_cfg.rewind_decay;
            idx = (idx - 1 + rd) % rd;
        }
        int8_t comp_dx = (int8_t)clampf(-wsum_dx, -127.0f, 127.0f);
        int8_t comp_dy = (int8_t)clampf(-wsum_dy, -127.0f, 127.0f);
        if (comp_dx != 0 || comp_dy != 0) {
            usb_composite_send_mouse_report(s_buttons, comp_dx, comp_dy, 0);
            ESP_LOGI(TAG, "rewind dx=%d dy=%d (%.1f weighted, %d samples)",
                     comp_dx, comp_dy, total_w, s_rewind_count);
        }
        s_rewind_count = 0;
        s_rewind_idx = 0;
    }

    s_tracking = tracking;
}

bool air_mouse_is_tracking(void)
{
    return s_tracking;
}

void air_mouse_set_buttons(uint8_t buttons)
{
    s_buttons = buttons;
}

float air_mouse_get_accumulated_movement(void)
{
    return s_accumulated_movement;
}
