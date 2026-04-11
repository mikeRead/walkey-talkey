#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QMI8658_ADDRESS_LOW  0x6A
#define QMI8658_ADDRESS_HIGH 0x6B

#define QMI8658_ONE_G (9.807f)

#define QMI8658_DISABLE_ALL  0x00
#define QMI8658_ENABLE_ACCEL 0x01
#define QMI8658_ENABLE_GYRO  0x02

typedef enum {
    QMI8658_WHO_AM_I    = 0x00,
    QMI8658_REVISION    = 0x01,
    QMI8658_CTRL1       = 0x02,
    QMI8658_CTRL2       = 0x03,
    QMI8658_CTRL3       = 0x04,
    QMI8658_CTRL5       = 0x06,
    QMI8658_CTRL7       = 0x08,
    QMI8658_STATUS0     = 0x2E,
    QMI8658_TIMESTAMP_L = 0x30,
    QMI8658_TIMESTAMP_M = 0x31,
    QMI8658_TIMESTAMP_H = 0x32,
    QMI8658_TEMP_L      = 0x33,
    QMI8658_TEMP_H      = 0x34,
    QMI8658_AX_L        = 0x35,
    QMI8658_GX_L        = 0x3B,
} qmi8658_register_t;

typedef enum {
    QMI8658_ACCEL_RANGE_2G  = 0x00,
    QMI8658_ACCEL_RANGE_4G  = 0x01,
    QMI8658_ACCEL_RANGE_8G  = 0x02,
    QMI8658_ACCEL_RANGE_16G = 0x03,
} qmi8658_accel_range_t;

typedef enum {
    QMI8658_ACCEL_ODR_8000HZ = 0x00,
    QMI8658_ACCEL_ODR_4000HZ = 0x01,
    QMI8658_ACCEL_ODR_2000HZ = 0x02,
    QMI8658_ACCEL_ODR_1000HZ = 0x03,
    QMI8658_ACCEL_ODR_500HZ  = 0x04,
    QMI8658_ACCEL_ODR_250HZ  = 0x05,
    QMI8658_ACCEL_ODR_125HZ  = 0x06,
    QMI8658_ACCEL_ODR_62_5HZ = 0x07,
} qmi8658_accel_odr_t;

typedef enum {
    QMI8658_GYRO_RANGE_32DPS   = 0x00,
    QMI8658_GYRO_RANGE_64DPS   = 0x01,
    QMI8658_GYRO_RANGE_128DPS  = 0x02,
    QMI8658_GYRO_RANGE_256DPS  = 0x03,
    QMI8658_GYRO_RANGE_512DPS  = 0x04,
    QMI8658_GYRO_RANGE_1024DPS = 0x05,
    QMI8658_GYRO_RANGE_2048DPS = 0x06,
} qmi8658_gyro_range_t;

typedef enum {
    QMI8658_GYRO_ODR_8000HZ = 0x00,
    QMI8658_GYRO_ODR_4000HZ = 0x01,
    QMI8658_GYRO_ODR_2000HZ = 0x02,
    QMI8658_GYRO_ODR_1000HZ = 0x03,
    QMI8658_GYRO_ODR_500HZ  = 0x04,
    QMI8658_GYRO_ODR_250HZ  = 0x05,
    QMI8658_GYRO_ODR_125HZ  = 0x06,
    QMI8658_GYRO_ODR_62_5HZ = 0x07,
} qmi8658_gyro_odr_t;

typedef struct {
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    float temperature;
    uint32_t timestamp;
} qmi8658_data_t;

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    uint16_t accel_lsb_div;
    uint16_t gyro_lsb_div;
    uint32_t timestamp;
} qmi8658_dev_t;

/**
 * Initialise the QMI8658 on the given I2C bus.
 * Verifies WHO_AM_I, configures default ranges/ODRs, and enables both sensors.
 */
esp_err_t qmi8658_init(qmi8658_dev_t *dev, i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr);

esp_err_t qmi8658_set_accel_range(qmi8658_dev_t *dev, qmi8658_accel_range_t range);
esp_err_t qmi8658_set_accel_odr(qmi8658_dev_t *dev, qmi8658_accel_odr_t odr);
esp_err_t qmi8658_set_gyro_range(qmi8658_dev_t *dev, qmi8658_gyro_range_t range);
esp_err_t qmi8658_set_gyro_odr(qmi8658_dev_t *dev, qmi8658_gyro_odr_t odr);
esp_err_t qmi8658_enable_sensors(qmi8658_dev_t *dev, uint8_t enable_flags);

/** Read gyroscope in degrees-per-second. */
esp_err_t qmi8658_read_gyro(qmi8658_dev_t *dev, float *x, float *y, float *z);

/** Read accelerometer + gyro + temperature in one burst. */
esp_err_t qmi8658_read_sensor_data(qmi8658_dev_t *dev, qmi8658_data_t *data);

esp_err_t qmi8658_is_data_ready(qmi8658_dev_t *dev, bool *ready);
esp_err_t qmi8658_get_who_am_i(qmi8658_dev_t *dev, uint8_t *who_am_i);
esp_err_t qmi8658_reset(qmi8658_dev_t *dev);

esp_err_t qmi8658_write_register(qmi8658_dev_t *dev, uint8_t reg, uint8_t value);
esp_err_t qmi8658_read_register(qmi8658_dev_t *dev, uint8_t reg, uint8_t *buffer, uint8_t length);

#ifdef __cplusplus
}
#endif
