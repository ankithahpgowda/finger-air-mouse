/**
 * =========================================================================
 * Project: Bluetooth Finger Mouse (ESP32-C3 Super Mini + MPU6050)
 * File:    config.h
 * Description: Master configuration file containing hardware pinout,
 *              sensor axis mapping, motion sensitivity, deadzone filtering,
 *              smoothing parameters, debouncing, and BLE settings.
 * =========================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =========================================================================
// 1. HARDWARE PIN DEFINITIONS (ESP32-C3 Super Mini)
// =========================================================================
// I2C Pins for MPU6050 (GY-521)
#define PIN_I2C_SDA             4   // GPIO 4 (ADC1_CH4) -> MPU6050 SDA
#define PIN_I2C_SCL             5   // GPIO 5 (ADC2_CH0) -> MPU6050 SCL

// Tactile Push Buttons (Connected between GPIO and GND, active LOW)
#define PIN_BUTTON_LEFT         6   // GPIO 6 (FSPI SCK) -> Left Mouse Click
#define PIN_BUTTON_RIGHT        7   // GPIO 7 (FSPI MOSI) -> Right Mouse Click

// Onboard Status LED (ESP32-C3 Super Mini onboard LED is on GPIO 8, active LOW)
#define PIN_STATUS_LED          8   // GPIO 8 -> Onboard Blue LED (LOW = ON, HIGH = OFF)
#define STATUS_LED_ACTIVE_LOW   true

// =========================================================================
// 2. I2C & MPU6050 CONFIGURATION
// =========================================================================
#define MPU6050_I2C_ADDR        0x68    // I2C address with AD0 tied to GND (0x69 if AD0 is VCC)
#define I2C_CLOCK_SPEED         400000UL // 400 kHz Fast-Mode I2C for low latency

// MPU6050 Internal Low-Pass Filter (DLPF) setting:
// 0: 260Hz, 1: 184Hz, 2: 94Hz, 3: 44Hz, 4: 21Hz, 5: 10Hz, 6: 5Hz
// Setting 3 (~44Hz bandwidth) eliminates high-frequency mechanical vibrations
#define MPU6050_DLPF_CFG        3

// Gyroscope Full-Scale Range:
// 0: +/-250 deg/s (131.0 LSB/(deg/s))
// 1: +/-500 deg/s (65.5 LSB/(deg/s))  <-- Recommended for finger air mouse
// 2: +/-1000 deg/s (32.8 LSB/(deg/s))
// 3: +/-2000 deg/s (16.4 LSB/(deg/s))
#define MPU6050_GYRO_FS         1
#define GYRO_SCALE_FACTOR       65.5f   // LSB per (deg/s) for +/-500 deg/s

// =========================================================================
// 3. SENSOR AXIS MAPPING & INVERSION
// =========================================================================
// Gyro axis selection enum:
//   AXIS_X = Roll  (rotation around longitudinal axis of sensor)
//   AXIS_Y = Pitch (rotation around transverse axis of sensor)
//   AXIS_Z = Yaw   (rotation around vertical axis of sensor)
enum GyroAxisSelect {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2
};

// Default mounting: MPU6050 worn on top of index finger pointing forward:
// - Turning finger Left/Right rotates around Z (Yaw) -> maps to cursor X
// - Tilting finger Up/Down rotates around Y (Pitch)   -> maps to cursor Y
#define AXIS_MAP_X              AXIS_Z
#define AXIS_MAP_Y              AXIS_Y

// Axis direction inversion:
// Adjust true/false if horizontal or vertical motion is reversed for your mount
#define INVERT_X                false
#define INVERT_Y                true   // Tilting finger downwards moves cursor down

// =========================================================================
// 4. MOTION FILTERING & SENSITIVITY
// =========================================================================
// Dead-zone threshold (in degrees per second):
// Any gyro angular velocity below this threshold is treated as 0.
// This completely stops cursor drift and unwanted jitter when finger is stationary.
#define DEADZONE_THRESHOLD_DPS  1.5f

// Exponential Moving Average (EMA) smoothing factor:
// Range: 0.05 (heavy smoothing, more latency) to 1.0 (raw sensor, maximum jitter)
// 0.35f offers an ideal balance of jitter reduction and sharp responsiveness.
#define SMOOTHING_FACTOR        0.35f

// Cursor Sensitivity Multipliers:
// Higher values move the cursor faster per degree of finger tilt.
#define SENSITIVITY_X           2.2f
#define SENSITIVITY_Y           2.2f

// Dynamic Cursor Acceleration:
// When enabled, slow movements allow pixel-level precision,
// while fast flicks accelerate across wide desktop monitors.
#define ENABLE_ACCELERATION     true
#define ACCEL_EXPONENT          1.22f   // Non-linear power curve exponent

// =========================================================================
// 5. STARTUP GYROSCOPE CALIBRATION
// =========================================================================
// Number of samples collected at startup while the device is kept stationary.
// 400 samples at ~4ms intervals = ~1.6 seconds calibration time.
#define CALIBRATION_SAMPLES     400
#define CALIBRATION_SAMPLE_DELAY_MS 4

// =========================================================================
// 6. BUTTON DEBOUNCING
// =========================================================================
// Debounce interval in milliseconds to prevent false / repeated switch contacts
#define BUTTON_DEBOUNCE_MS      35

// =========================================================================
// 7. BLUETOOTH LE (BLE) HID SETTINGS
// =========================================================================
#define BLE_DEVICE_NAME         "Finger Mouse"
#define BLE_MANUFACTURER        "DIY Embedded"
#define BLE_BATTERY_LEVEL       100

// BLE Mouse backend selector:
// 0: Use built-in lightweight NimBLE HID Mouse implementation in mouse.cpp (Recommended! Requires only NimBLE-Arduino).
// 1: Use external BleMouse.h library (if you already have ESP32-NimBLE-Mouse installed).
#define USE_EXTERNAL_BLE_MOUSE_LIB  0

// =========================================================================
// 8. TIMING & POWER MANAGEMENT
// =========================================================================
// Active update interval when connected (in ms):
// 10ms = 100 Hz report rate (ultra-smooth and responsive cursor).
#define ACTIVE_UPDATE_INTERVAL_MS   10

// Idle update interval when disconnected / waiting for Bluetooth (in ms):
// 250ms = 4 Hz polling rate to conserve battery power while advertising.
#define IDLE_UPDATE_INTERVAL_MS     250

// Serial Diagnostics
#define DEBUG_SERIAL            true
#define SERIAL_BAUD_RATE        115200

#endif // CONFIG_H
