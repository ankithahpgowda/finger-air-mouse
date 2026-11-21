/**
 * =========================================================================
 * Project: Bluetooth Finger Mouse (ESP32-C3 Super Mini + MPU6050)
 * File:    sensor.cpp
 * Description: Implementation of MPU6050 I2C communication, register
 *              configuration, gyroscope zero-rate calibration,
 *              noise reduction, dead-band filtering, and 2D motion mapping.
 * =========================================================================
 */

#include "sensor.h"
#include <math.h>

MPU6050Sensor::MPU6050Sensor()
    : _sdaPin(PIN_I2C_SDA),
      _sclPin(PIN_I2C_SCL),
      _i2cSpeed(I2C_CLOCK_SPEED),
      _i2cAddr(MPU6050_I2C_ADDR),
      _offsetGx(0.0f),
      _offsetGy(0.0f),
      _offsetGz(0.0f),
      _filteredX(0.0f),
      _filteredY(0.0f),
      _accumX(0.0f),
      _accumY(0.0f),
      _consecutiveErrors(0) {}

bool MPU6050Sensor::begin(uint8_t sdaPin, uint8_t sclPin, uint32_t speed, uint8_t addr) {
    _sdaPin   = sdaPin;
    _sclPin   = sclPin;
    _i2cSpeed = speed;
    _i2cAddr  = addr;

    // Initialize Wire library with specified ESP32-C3 GPIO pins
    Wire.begin(_sdaPin, _sclPin, _i2cSpeed);
    Wire.setTimeOut(50); // Set timeout to 50ms to prevent blocking

    // Verify physical connection via WHO_AM_I register (0x75)
    uint8_t whoAmI = 0;
    if (!readRegisters(MPU6050_REG_WHO_AM_I, &whoAmI, 1)) {
        #if DEBUG_SERIAL
        Serial.println("[MPU6050] ERROR: No response on I2C bus!");
        #endif
        return false;
    }

    if (whoAmI != 0x68 && whoAmI != 0x69) {
        #if DEBUG_SERIAL
        Serial.printf("[MPU6050] WARNING: Unexpected WHO_AM_I value: 0x%02X (Expected 0x68)\n", whoAmI);
        #endif
        // Some clones report 0x70, 0x72 or 0x98; continue if device acknowledged read
    } else {
        #if DEBUG_SERIAL
        Serial.printf("[MPU6050] Successfully detected sensor (WHO_AM_I = 0x%02X)\n", whoAmI);
        #endif
    }

    // Step 1: Wake up MPU6050 (Clears Sleep bit, sets Clock Source to X-Gyro PLL for high stability)
    // PWR_MGMT_1: CLKSEL = 0x01 (PLL with X axis gyroscope reference)
    if (!writeRegister(MPU6050_REG_PWR_MGMT_1, 0x01)) {
        return false;
    }
    delay(10);

    // Step 2: Configure Sample Rate Divider (SMPLRT_DIV = 0 -> 1kHz internal sample rate)
    if (!writeRegister(MPU6050_REG_SMPLRT_DIV, 0x00)) {
        return false;
    }

    // Step 3: Configure Digital Low Pass Filter (DLPF)
    // Register 0x1A: bits 2:0 set DLPF_CFG (3 = ~44 Hz bandwidth, 4.8ms delay)
    // This hardware filter effectively eliminates high-frequency mechanical noise & finger tremor
    if (!writeRegister(MPU6050_REG_CONFIG, MPU6050_DLPF_CFG & 0x07)) {
        return false;
    }

    // Step 4: Configure Gyroscope Full Scale Range
    // Register 0x1B: bits 4:3 set FS_SEL (1 = +/-500 deg/s)
    uint8_t gyroConfigVal = (MPU6050_GYRO_FS & 0x03) << 3;
    if (!writeRegister(MPU6050_REG_GYRO_CONFIG, gyroConfigVal)) {
        return false;
    }

    // Step 5: Configure Accelerometer Full Scale Range
    // Register 0x1C: bits 4:3 set AFS_SEL (0 = +/-2g)
    if (!writeRegister(MPU6050_REG_ACCEL_CONFIG, 0x00)) {
        return false;
    }

    delay(20);

    #if DEBUG_SERIAL
    Serial.println("[MPU6050] Hardware registers configured successfully.");
    #endif

    _consecutiveErrors = 0;
    return true;
}

bool MPU6050Sensor::testConnection() {
    uint8_t who = 0;
    if (readRegisters(MPU6050_REG_WHO_AM_I, &who, 1)) {
        return (who == 0x68 || who == 0x69 || who != 0x00);
    }
    return false;
}

void MPU6050Sensor::calibrateGyro(uint16_t samples, int8_t ledPin) {
    #if DEBUG_SERIAL
    Serial.printf("[CALIBRATION] Starting Gyroscope Calibration (%d samples)...\n", samples);
    Serial.println("[CALIBRATION] >> IMPORTANT: Keep your finger / device completely still! <<");
    #endif

    double sumGx = 0.0;
    double sumGy = 0.0;
    double sumGz = 0.0;
    uint16_t validCount = 0;

    for (uint16_t i = 0; i < samples; i++) {
        uint8_t buffer[6];
        if (readRegisters(MPU6050_REG_GYRO_XOUT_H, buffer, 6)) {
            int16_t rawGx = (int16_t)((buffer[0] << 8) | buffer[1]);
            int16_t rawGy = (int16_t)((buffer[2] << 8) | buffer[3]);
            int16_t rawGz = (int16_t)((buffer[4] << 8) | buffer[5]);

            sumGx += (double)rawGx;
            sumGy += (double)rawGy;
            sumGz += (double)rawGz;
            validCount++;
        }

        // Rapid blink LED to indicate ongoing calibration
        if (ledPin >= 0 && (i % 25 == 0)) {
            digitalWrite(ledPin, (i / 25) % 2 == 0 ? LOW : HIGH);
        }

        delay(CALIBRATION_SAMPLE_DELAY_MS);
    }

    if (validCount > (samples / 2)) {
        // Average the raw readings and convert to degrees per second
        _offsetGx = (float)(sumGx / validCount) / GYRO_SCALE_FACTOR;
        _offsetGy = (float)(sumGy / validCount) / GYRO_SCALE_FACTOR;
        _offsetGz = (float)(sumGz / validCount) / GYRO_SCALE_FACTOR;

        #if DEBUG_SERIAL
        Serial.println("[CALIBRATION] Calibration Complete!");
        Serial.printf("[CALIBRATION] Offsets (deg/s) -> X: %.2f | Y: %.2f | Z: %.2f (from %d samples)\n", 
                      _offsetGx, _offsetGy, _offsetGz, validCount);
        #endif
    } else {
        #if DEBUG_SERIAL
        Serial.println("[CALIBRATION] ERROR: Too many I2C read failures during calibration!");
        #endif
    }

    // Reset filters and subpixel accumulators
    _filteredX = 0.0f;
    _filteredY = 0.0f;
    _accumX    = 0.0f;
    _accumY    = 0.0f;

    // Turn LED off (HIGH for active-low)
    if (ledPin >= 0) {
        digitalWrite(ledPin, HIGH);
    }
}

bool MPU6050Sensor::readSensor(float &gx, float &gy, float &gz, float &ax, float &ay, float &az) {
    // Burst read 14 consecutive registers starting at 0x3B:
    // 0x3B-0x40: Accel X, Y, Z (6 bytes)
    // 0x41-0x42: Temperature (2 bytes)
    // 0x43-0x48: Gyro X, Y, Z (6 bytes)
    uint8_t rawBuffer[14];
    if (!readRegisters(MPU6050_REG_ACCEL_XOUT_H, rawBuffer, 14)) {
        _consecutiveErrors++;
        if (_consecutiveErrors > 10) {
            #if DEBUG_SERIAL
            Serial.println("[MPU6050] Repeated I2C bus errors! Attempting bus reset...");
            #endif
            resetI2C();
            _consecutiveErrors = 0;
        }
        return false;
    }

    _consecutiveErrors = 0;

    // Parse Accelerometer (Signed 16-bit big-endian, +/-2g scale = 16384 LSB/g)
    int16_t rawAx = (int16_t)((rawBuffer[0] << 8) | rawBuffer[1]);
    int16_t rawAy = (int16_t)((rawBuffer[2] << 8) | rawBuffer[3]);
    int16_t rawAz = (int16_t)((rawBuffer[4] << 8) | rawBuffer[5]);

    ax = (float)rawAx / 16384.0f;
    ay = (float)rawAy / 16384.0f;
    az = (float)rawAz / 16384.0f;

    // Parse Gyroscope (Signed 16-bit big-endian, +/-500 deg/s scale = 65.5 LSB/(deg/s))
    int16_t rawGx = (int16_t)((rawBuffer[8]  << 8) | rawBuffer[9]);
    int16_t rawGy = (int16_t)((rawBuffer[10] << 8) | rawBuffer[11]);
    int16_t rawGz = (int16_t)((rawBuffer[12] << 8) | rawBuffer[13]);

    // Subtract calibrated zero-rate bias offsets
    gx = ((float)rawGx / GYRO_SCALE_FACTOR) - _offsetGx;
    gy = ((float)rawGy / GYRO_SCALE_FACTOR) - _offsetGy;
    gz = ((float)rawGz / GYRO_SCALE_FACTOR) - _offsetGz;

    return true;
}

void MPU6050Sensor::processMotion(float gx, float gy, float gz, int8_t &deltaX, int8_t &deltaY) {
    // 1. Map physical gyro axes to horizontal (X) and vertical (Y) cursor channels
    float rawInputX = 0.0f;
    float rawInputY = 0.0f;

    switch (AXIS_MAP_X) {
        case AXIS_X: rawInputX = gx; break;
        case AXIS_Y: rawInputX = gy; break;
        case AXIS_Z: rawInputX = gz; break;
    }

    switch (AXIS_MAP_Y) {
        case AXIS_X: rawInputY = gx; break;
        case AXIS_Y: rawInputY = gy; break;
        case AXIS_Z: rawInputY = gz; break;
    }

    // 2. Apply axis direction inversions
    if (INVERT_X) rawInputX = -rawInputX;
    if (INVERT_Y) rawInputY = -rawInputY;

    // 3. Dead-zone Filter:
    // If angular velocity is less than DEADZONE_THRESHOLD_DPS, zero it out completely.
    // If greater, subtract the threshold so movement starts smoothly from zero without a sudden jump.
    float deadbandX = 0.0f;
    if (fabs(rawInputX) > DEADZONE_THRESHOLD_DPS) {
        deadbandX = (rawInputX > 0.0f) ? (rawInputX - DEADZONE_THRESHOLD_DPS) : (rawInputX + DEADZONE_THRESHOLD_DPS);
    }

    float deadbandY = 0.0f;
    if (fabs(rawInputY) > DEADZONE_THRESHOLD_DPS) {
        deadbandY = (rawInputY > 0.0f) ? (rawInputY - DEADZONE_THRESHOLD_DPS) : (rawInputY + DEADZONE_THRESHOLD_DPS);
    }

    // 4. Exponential Moving Average (EMA) Smoothing:
    // Smooths out hand micro-tremor and sensor noise
    _filteredX = (SMOOTHING_FACTOR * deadbandX) + ((1.0f - SMOOTHING_FACTOR) * _filteredX);
    _filteredY = (SMOOTHING_FACTOR * deadbandY) + ((1.0f - SMOOTHING_FACTOR) * _filteredY);

    // If filtered value is practically zero, clear it to prevent endless tiny residual motion
    if (fabs(_filteredX) < 0.05f) _filteredX = 0.0f;
    if (fabs(_filteredY) < 0.05f) _filteredY = 0.0f;

    // 5. Sensitivity and Non-linear Acceleration Curve:
    // Small rotations remain precise; fast flicks accelerate across the screen.
    float speedX = fabs(_filteredX) * SENSITIVITY_X;
    float speedY = fabs(_filteredY) * SENSITIVITY_Y;

    if (ENABLE_ACCELERATION) {
        if (speedX > 1.0f) speedX = powf(speedX, ACCEL_EXPONENT);
        if (speedY > 1.0f) speedY = powf(speedY, ACCEL_EXPONENT);
    }

    float velocityX = (_filteredX >= 0.0f ? 1.0f : -1.0f) * speedX;
    float velocityY = (_filteredY >= 0.0f ? 1.0f : -1.0f) * speedY;

    // 6. Sub-pixel Fractional Movement Accumulator:
    // HID mouse reports must be signed 8-bit integers (-127 to +127).
    // Fractional accumulation ensures slow, precise movements are not truncated to zero!
    _accumX += velocityX;
    _accumY += velocityY;

    int moveX = (int)_accumX;
    int moveY = (int)_accumY;

    // Constrain to signed 8-bit limits (-127 to 127)
    deltaX = (int8_t)constrain(moveX, -127, 127);
    deltaY = (int8_t)constrain(moveY, -127, 127);

    // Retain remainder for the next update cycle
    _accumX -= (float)deltaX;
    _accumY -= (float)deltaY;
}

void MPU6050Sensor::resetI2C() {
    Wire.end();
    delay(10);
    // Bus clock-clearing sequence (toggles SCL 9 times in case a slave holds SDA low)
    pinMode(_sclPin, OUTPUT);
    pinMode(_sdaPin, INPUT_PULLUP);
    for (int i = 0; i < 9; i++) {
        digitalWrite(_sclPin, HIGH);
        delayMicroseconds(5);
        digitalWrite(_sclPin, LOW);
        delayMicroseconds(5);
    }
    digitalWrite(_sclPin, HIGH);
    delay(10);
    // Re-initialize Wire
    begin(_sdaPin, _sclPin, _i2cSpeed, _i2cAddr);
}

bool MPU6050Sensor::writeRegister(uint8_t reg, uint8_t data) {
    Wire.beginTransmission(_i2cAddr);
    Wire.write(reg);
    Wire.write(data);
    return (Wire.endTransmission() == 0);
}

bool MPU6050Sensor::readRegisters(uint8_t startReg, uint8_t *buffer, uint8_t length) {
    Wire.beginTransmission(_i2cAddr);
    Wire.write(startReg);
    if (Wire.endTransmission(false) != 0) { // Send restart, keep connection active
        return false;
    }

    uint8_t bytesReceived = Wire.requestFrom((uint8_t)_i2cAddr, (uint8_t)length, (uint8_t)true);
    if (bytesReceived != length) {
        return false;
    }

    for (uint8_t i = 0; i < length; i++) {
        buffer[i] = Wire.read();
    }
    return true;
}
