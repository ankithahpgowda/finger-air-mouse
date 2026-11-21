/**
 * =========================================================================
 * Project: Bluetooth Finger Mouse (ESP32-C3 Super Mini + MPU6050)
 * File:    sensor.h
 * Description: Declarations for MPU6050 accelerometer & gyroscope sensor
 *              handling, hardware I2C register interface, calibration,
 *              deadzone filtering, EMA smoothing, and motion mapping.
 * =========================================================================
 */

#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// MPU6050 Hardware Register Map
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_TEMP_OUT_H   0x41
#define MPU6050_REG_GYRO_XOUT_H  0x43
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_WHO_AM_I     0x75

class MPU6050Sensor {
public:
    MPU6050Sensor();

    /**
     * @brief Initialize I2C bus and configure MPU6050 internal registers.
     * @param sdaPin GPIO for I2C Data line
     * @param sclPin GPIO for I2C Clock line
     * @param speed I2C clock frequency (e.g. 400000UL)
     * @param addr I2C 7-bit device address (0x68 or 0x69)
     * @return true on success, false on I2C failure
     */
    bool begin(uint8_t sdaPin = PIN_I2C_SDA, 
               uint8_t sclPin = PIN_I2C_SCL, 
               uint32_t speed = I2C_CLOCK_SPEED, 
               uint8_t addr = MPU6050_I2C_ADDR);

    /**
     * @brief Checks if MPU6050 responds to WHO_AM_I query (0x75 -> 0x68).
     * @return true if device is responding correctly
     */
    bool testConnection();

    /**
     * @brief Performs gyroscope offset calibration while device is held stationary.
     * @param samples Number of readings to average
     * @param ledPin Optional status LED pin to blink during calibration
     */
    void calibrateGyro(uint16_t samples = CALIBRATION_SAMPLES, int8_t ledPin = PIN_STATUS_LED);

    /**
     * @brief Reads raw 14-byte sensor burst and converts to calibrated degrees/s and g's.
     * @param gx Calibrated Gyro X (deg/s)
     * @param gy Calibrated Gyro Y (deg/s)
     * @param gz Calibrated Gyro Z (deg/s)
     * @param ax Calibrated Accel X (g)
     * @param ay Calibrated Accel Y (g)
     * @param az Calibrated Accel Z (g)
     * @return true if I2C read was successful
     */
    bool readSensor(float &gx, float &gy, float &gz, float &ax, float &ay, float &az);

    /**
     * @brief Transforms calibrated 3-axis gyro rates into relative 2D mouse cursor movements (deltaX, deltaY).
     *        Applies deadzone filtering, exponential moving average smoothing, sensitivity scaling,
     *        optional acceleration curve, and fractional sub-pixel accumulation.
     * @param gx Gyro X rate (deg/s)
     * @param gy Gyro Y rate (deg/s)
     * @param gz Gyro Z rate (deg/s)
     * @param deltaX Output relative X displacement (-127 to +127)
     * @param deltaY Output relative Y displacement (-127 to +127)
     */
    void processMotion(float gx, float gy, float gz, int8_t &deltaX, int8_t &deltaY);

    /**
     * @brief Recovers from bus lockup by toggling SCL lines and re-initializing.
     */
    void resetI2C();

    // Offset getters
    float getOffsetGx() const { return _offsetGx; }
    float getOffsetGy() const { return _offsetGy; }
    float getOffsetGz() const { return _offsetGz; }

private:
    uint8_t  _sdaPin;
    uint8_t  _sclPin;
    uint32_t _i2cSpeed;
    uint8_t  _i2cAddr;

    // Calibration bias offsets (degrees per second)
    float _offsetGx;
    float _offsetGy;
    float _offsetGz;

    // Filter states
    float _filteredX;
    float _filteredY;

    // Sub-pixel fractional movement accumulators
    float _accumX;
    float _accumY;

    // Communication error tracker
    uint8_t _consecutiveErrors;

    // Low-level I2C register helpers
    bool writeRegister(uint8_t reg, uint8_t data);
    bool readRegisters(uint8_t startReg, uint8_t *buffer, uint8_t length);
};

#endif // SENSOR_H
