/*
 * QMI8658 6-axis IMU driver.
 * Vendored from Waveshare-ESP32-components/sensor/qmi8658 and trimmed to the
 * subset needed for air-mouse gyro reading.
 *
 * Original: https://github.com/waveshareteam/Waveshare-ESP32-components
 * License:  MIT
 */

#include "qmi8658.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "qmi8658";

/* -------------------------------------------------------------------------- */
/* Low-level register helpers                                                 */
/* -------------------------------------------------------------------------- */

esp_err_t qmi8658_write_register(qmi8658_dev_t *dev, uint8_t reg, uint8_t value)
{
    if ((dev == NULL) || (dev->dev_handle == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(dev->dev_handle, data, 2, 1000);
}

esp_err_t qmi8658_read_register(qmi8658_dev_t *dev, uint8_t reg, uint8_t *buffer, uint8_t length)
{
    if ((dev == NULL) || (buffer == NULL) || (length == 0) || (dev->dev_handle == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_transmit_receive(dev->dev_handle, &reg, 1, buffer, length, 1000);
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

esp_err_t qmi8658_init(qmi8658_dev_t *dev, i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr)
{
    if ((dev == NULL) || (bus_handle == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->bus_handle   = bus_handle;
    dev->accel_lsb_div = 4096;   /* 8G default */
    dev->gyro_lsb_div  = 64;     /* 512 DPS default */
    dev->timestamp     = 0;

    i2c_device_config_t dev_config = {
        .dev_addr_length    = I2C_ADDR_BIT_LEN_7,
        .device_address     = i2c_addr,
        .scl_speed_hz       = 400000,
        .scl_wait_us        = 0,
        .flags.disable_ack_check = false,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_config, &dev->dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to add I2C device: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t who_am_i;
    ret = qmi8658_get_who_am_i(dev, &who_am_i);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to read WHO_AM_I");
        return ret;
    }
    if (who_am_i != 0x05) {
        ESP_LOGE(TAG, "unexpected WHO_AM_I: 0x%02X (expected 0x05)", who_am_i);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02X -- QMI8658 detected at 0x%02X", who_am_i, i2c_addr);

    ret = qmi8658_write_register(dev, QMI8658_CTRL1, 0x60);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = qmi8658_set_accel_range(dev, QMI8658_ACCEL_RANGE_8G);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = qmi8658_set_accel_odr(dev, QMI8658_ACCEL_ODR_125HZ);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = qmi8658_set_gyro_range(dev, QMI8658_GYRO_RANGE_512DPS);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = qmi8658_set_gyro_odr(dev, QMI8658_GYRO_ODR_125HZ);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = qmi8658_enable_sensors(dev, QMI8658_ENABLE_ACCEL | QMI8658_ENABLE_GYRO);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "QMI8658 initialised (accel 8G/125Hz, gyro 512DPS/125Hz)");
    return ESP_OK;
}

esp_err_t qmi8658_set_accel_range(qmi8658_dev_t *dev, qmi8658_accel_range_t range)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (range) {
        case QMI8658_ACCEL_RANGE_2G:  dev->accel_lsb_div = 16384; break;
        case QMI8658_ACCEL_RANGE_4G:  dev->accel_lsb_div = 8192;  break;
        case QMI8658_ACCEL_RANGE_8G:  dev->accel_lsb_div = 4096;  break;
        case QMI8658_ACCEL_RANGE_16G: dev->accel_lsb_div = 2048;  break;
        default: return ESP_ERR_INVALID_ARG;
    }
    return qmi8658_write_register(dev, QMI8658_CTRL2, ((uint8_t)range << 4) | 0x06);
}

esp_err_t qmi8658_set_accel_odr(qmi8658_dev_t *dev, qmi8658_accel_odr_t odr)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t current;
    esp_err_t ret = qmi8658_read_register(dev, QMI8658_CTRL2, &current, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    return qmi8658_write_register(dev, QMI8658_CTRL2, (current & 0xF0) | (uint8_t)odr);
}

esp_err_t qmi8658_set_gyro_range(qmi8658_dev_t *dev, qmi8658_gyro_range_t range)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (range) {
        case QMI8658_GYRO_RANGE_32DPS:   dev->gyro_lsb_div = 1024; break;
        case QMI8658_GYRO_RANGE_64DPS:   dev->gyro_lsb_div = 512;  break;
        case QMI8658_GYRO_RANGE_128DPS:  dev->gyro_lsb_div = 256;  break;
        case QMI8658_GYRO_RANGE_256DPS:  dev->gyro_lsb_div = 128;  break;
        case QMI8658_GYRO_RANGE_512DPS:  dev->gyro_lsb_div = 64;   break;
        case QMI8658_GYRO_RANGE_1024DPS: dev->gyro_lsb_div = 32;   break;
        case QMI8658_GYRO_RANGE_2048DPS: dev->gyro_lsb_div = 16;   break;
        default: return ESP_ERR_INVALID_ARG;
    }
    return qmi8658_write_register(dev, QMI8658_CTRL3, ((uint8_t)range << 4) | 0x06);
}

esp_err_t qmi8658_set_gyro_odr(qmi8658_dev_t *dev, qmi8658_gyro_odr_t odr)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t current;
    esp_err_t ret = qmi8658_read_register(dev, QMI8658_CTRL3, &current, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    return qmi8658_write_register(dev, QMI8658_CTRL3, (current & 0xF0) | (uint8_t)odr);
}

esp_err_t qmi8658_enable_sensors(qmi8658_dev_t *dev, uint8_t enable_flags)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return qmi8658_write_register(dev, QMI8658_CTRL7, enable_flags & 0x0F);
}

esp_err_t qmi8658_read_gyro(qmi8658_dev_t *dev, float *x, float *y, float *z)
{
    if ((dev == NULL) || (x == NULL) || (y == NULL) || (z == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[6];
    esp_err_t ret = qmi8658_read_register(dev, QMI8658_GX_L, buf, 6);
    if (ret != ESP_OK) {
        return ret;
    }

    int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);

    *x = (float)raw_x / dev->gyro_lsb_div;
    *y = (float)raw_y / dev->gyro_lsb_div;
    *z = (float)raw_z / dev->gyro_lsb_div;

    return ESP_OK;
}

esp_err_t qmi8658_read_sensor_data(qmi8658_dev_t *dev, qmi8658_data_t *data)
{
    if ((dev == NULL) || (data == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t ts_buf[3];
    esp_err_t ret = qmi8658_read_register(dev, QMI8658_TIMESTAMP_L, ts_buf, 3);
    if (ret == ESP_OK) {
        uint32_t ts = ((uint32_t)ts_buf[2] << 16) | ((uint32_t)ts_buf[1] << 8) | ts_buf[0];
        if (ts > dev->timestamp) {
            dev->timestamp = ts;
        } else {
            dev->timestamp = ts + 0x1000000 - dev->timestamp;
        }
        data->timestamp = dev->timestamp;
    }

    uint8_t sensor_buf[12];
    ret = qmi8658_read_register(dev, QMI8658_AX_L, sensor_buf, 12);
    if (ret != ESP_OK) {
        return ret;
    }

    int16_t raw_ax = (int16_t)((sensor_buf[1]  << 8) | sensor_buf[0]);
    int16_t raw_ay = (int16_t)((sensor_buf[3]  << 8) | sensor_buf[2]);
    int16_t raw_az = (int16_t)((sensor_buf[5]  << 8) | sensor_buf[4]);
    int16_t raw_gx = (int16_t)((sensor_buf[7]  << 8) | sensor_buf[6]);
    int16_t raw_gy = (int16_t)((sensor_buf[9]  << 8) | sensor_buf[8]);
    int16_t raw_gz = (int16_t)((sensor_buf[11] << 8) | sensor_buf[10]);

    data->accel_x = (raw_ax * 1000.0f) / dev->accel_lsb_div;
    data->accel_y = (raw_ay * 1000.0f) / dev->accel_lsb_div;
    data->accel_z = (raw_az * 1000.0f) / dev->accel_lsb_div;

    data->gyro_x = (float)raw_gx / dev->gyro_lsb_div;
    data->gyro_y = (float)raw_gy / dev->gyro_lsb_div;
    data->gyro_z = (float)raw_gz / dev->gyro_lsb_div;

    uint8_t temp_buf[2];
    ret = qmi8658_read_register(dev, QMI8658_TEMP_L, temp_buf, 2);
    if (ret == ESP_OK) {
        int16_t raw_temp = (int16_t)((temp_buf[1] << 8) | temp_buf[0]);
        data->temperature = (float)raw_temp / 256.0f;
    }

    return ESP_OK;
}

esp_err_t qmi8658_is_data_ready(qmi8658_dev_t *dev, bool *ready)
{
    if ((dev == NULL) || (ready == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t status;
    esp_err_t ret = qmi8658_read_register(dev, QMI8658_STATUS0, &status, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    *ready = (status & 0x03) != 0;
    return ESP_OK;
}

esp_err_t qmi8658_get_who_am_i(qmi8658_dev_t *dev, uint8_t *who_am_i)
{
    if ((dev == NULL) || (who_am_i == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    return qmi8658_read_register(dev, QMI8658_WHO_AM_I, who_am_i, 1);
}

esp_err_t qmi8658_reset(qmi8658_dev_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return qmi8658_write_register(dev, QMI8658_CTRL1, 0x80);
}
