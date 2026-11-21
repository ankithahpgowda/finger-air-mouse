/**
 * =========================================================================
 * Project:  Bluetooth Finger Mouse
 * Hardware: ESP32-C3 Super Mini + MPU6050 (GY-521) + 2 Tactile Buttons
 * File:     finger_mouse.ino
 * Description: Main firmware for ESP32-C3 wearable Bluetooth LE finger mouse.
 *              Captures 6-axis IMU angular velocity, performs zero-rate
 *              gyro calibration, applies deadzone filtering and EMA
 *              smoothing, debounces tactile click buttons, and streams
 *              standard HID mouse reports wirelessly to paired computers.
 * =========================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "sensor.h"
#include "mouse.h"

// -------------------------------------------------------------------------
// Global Object & State Definitions
// -------------------------------------------------------------------------
static MPU6050Sensor s_sensor;

// Structure for non-blocking button debouncing
struct ButtonControl {
    uint8_t       pin;
    uint8_t       mouseMask;
    bool          lastRawReading;
    bool          isPressed;
    unsigned long lastDebounceTime;
    const char*   name;
};

static ButtonControl s_btnLeft = {
    PIN_BUTTON_LEFT,
    MOUSE_BUTTON_LEFT,
    HIGH,   // Normal unpressed state with INPUT_PULLUP
    false,
    0,
    "Left Click"
};

static ButtonControl s_btnRight = {
    PIN_BUTTON_RIGHT,
    MOUSE_BUTTON_RIGHT,
    HIGH,   // Normal unpressed state with INPUT_PULLUP
    false,
    0,
    "Right Click"
};

// Timing trackers for non-blocking loop execution
static unsigned long s_lastPollTime    = 0;
static unsigned long s_lastLedTime     = 0;
static bool          s_lastConnected   = false;
static bool          s_ledState        = false;

// -------------------------------------------------------------------------
// Button Handler (Non-blocking Debounce & Press/Release Dispatcher)
// -------------------------------------------------------------------------
static void updateButton(ButtonControl &btn) {
    // Read physical pin (active LOW due to INPUT_PULLUP)
    bool rawReading = (digitalRead(btn.pin) == LOW);

    // If reading changed, reset debounce timer
    if (rawReading != btn.lastRawReading) {
        btn.lastDebounceTime = millis();
        btn.lastRawReading   = rawReading;
    }

    // Evaluate state once signal has remained stable for debounce interval
    if ((millis() - btn.lastDebounceTime) >= BUTTON_DEBOUNCE_MS) {
        if (rawReading != btn.isPressed) {
            btn.isPressed = rawReading;

            if (bleMouseIsConnected()) {
                if (btn.isPressed) {
                    bleMousePress(btn.mouseMask);
                    #if DEBUG_SERIAL
                    Serial.printf("[BUTTON] %s: PRESSED\n", btn.name);
                    #endif
                } else {
                    bleMouseRelease(btn.mouseMask);
                    #if DEBUG_SERIAL
                    Serial.printf("[BUTTON] %s: RELEASED\n", btn.name);
                    #endif
                }
            }
        }
    }
}

// -------------------------------------------------------------------------
// Status LED Handler
// -------------------------------------------------------------------------
static void updateStatusLED(bool connected) {
    unsigned long now = millis();

    if (connected) {
        // Solid ON when connected and ready
        digitalWrite(PIN_STATUS_LED, STATUS_LED_ACTIVE_LOW ? LOW : HIGH);
    } else {
        // Slow pulsing blink (500ms toggle) while waiting for Bluetooth connection
        if (now - s_lastLedTime >= 500) {
            s_lastLedTime = now;
            s_ledState = !s_ledState;
            digitalWrite(PIN_STATUS_LED, s_ledState ? LOW : HIGH);
        }
    }
}

// -------------------------------------------------------------------------
// Arduino Setup Routine
// -------------------------------------------------------------------------
void setup() {
    // 1. Initialize Serial Port for Diagnostics
    #if DEBUG_SERIAL
    Serial.begin(SERIAL_BAUD_RATE);
    // Allow USB CDC on ESP32-C3 brief time to connect if plugged into PC
    unsigned long serialStart = millis();
    while (!Serial && (millis() - serialStart < 2000)) {
        delay(10);
    }
    Serial.println();
    Serial.println("==================================================");
    Serial.println("         BLUETOOTH FINGER AIR MOUSE               ");
    Serial.println("      ESP32-C3 Super Mini + MPU6050 (GY-521)      ");
    Serial.println("==================================================");
    Serial.printf("[INFO] CPU Frequency: %u MHz\n", getCpuFrequencyMhz());
    Serial.printf("[INFO] Left Button:  GPIO %d (INPUT_PULLUP)\n", PIN_BUTTON_LEFT);
    Serial.printf("[INFO] Right Button: GPIO %d (INPUT_PULLUP)\n", PIN_BUTTON_RIGHT);
    Serial.printf("[INFO] I2C SDA:      GPIO %d\n", PIN_I2C_SDA);
    Serial.printf("[INFO] I2C SCL:      GPIO %d\n", PIN_I2C_SCL);
    Serial.printf("[INFO] Status LED:   GPIO %d\n", PIN_STATUS_LED);
    #endif

    // 2. Configure Hardware GPIO Pins
    pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
    pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);
    pinMode(PIN_STATUS_LED, OUTPUT);

    // Initial LED state: OFF (HIGH for active-low)
    digitalWrite(PIN_STATUS_LED, STATUS_LED_ACTIVE_LOW ? HIGH : LOW);

    // 3. Initialize MPU6050 6-Axis Motion Sensor
    #if DEBUG_SERIAL
    Serial.println("[INIT] Initializing MPU6050 sensor...");
    #endif

    bool sensorReady = false;
    uint8_t retryCount = 0;
    while (!sensorReady && retryCount < 5) {
        sensorReady = s_sensor.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_SPEED, MPU6050_I2C_ADDR);
        if (!sensorReady) {
            retryCount++;
            #if DEBUG_SERIAL
            Serial.printf("[INIT] Retrying MPU6050 connection (%d/5)...\n", retryCount);
            #endif
            delay(300);
        }
    }

    if (!sensorReady) {
        #if DEBUG_SERIAL
        Serial.println("[CRITICAL ERROR] Failed to communicate with MPU6050!");
        Serial.println("[CHECK] 1. Verify SDA is on GPIO 4 and SCL is on GPIO 5.");
        Serial.println("[CHECK] 2. Verify VCC is 3.3V and GND is connected.");
        Serial.println("[CHECK] 3. Verify AD0 is connected to GND (Address 0x68).");
        #endif
        // Blink error code indefinitely (rapid double-blink)
        while (true) {
            digitalWrite(PIN_STATUS_LED, LOW); delay(100);
            digitalWrite(PIN_STATUS_LED, HIGH); delay(100);
            digitalWrite(PIN_STATUS_LED, LOW); delay(100);
            digitalWrite(PIN_STATUS_LED, HIGH); delay(700);
        }
    }

    // 4. Perform Startup Gyroscope Calibration
    // Device must be placed on a steady surface or held still on finger
    s_sensor.calibrateGyro(CALIBRATION_SAMPLES, PIN_STATUS_LED);

    // 5. Initialize Bluetooth Low Energy (BLE) HID Mouse Service
    #if DEBUG_SERIAL
    Serial.println("[INIT] Initializing BLE HID Mouse service...");
    #endif

    if (!bleMouseInit(BLE_DEVICE_NAME, BLE_MANUFACTURER, BLE_BATTERY_LEVEL)) {
        #if DEBUG_SERIAL
        Serial.println("[CRITICAL ERROR] Failed to initialize BLE HID stack!");
        #endif
        while (true) {
            digitalWrite(PIN_STATUS_LED, LOW); delay(500);
            digitalWrite(PIN_STATUS_LED, HIGH); delay(500);
        }
    }

    #if DEBUG_SERIAL
    Serial.println("==================================================");
    Serial.println("[READY] Finger Mouse is broadcasting as: 'Finger Mouse'");
    Serial.println("[READY] Open your laptop Bluetooth settings to pair.");
    Serial.println("==================================================");
    #endif
}

// -------------------------------------------------------------------------
// Arduino Main Loop (Completely Non-Blocking)
// -------------------------------------------------------------------------
void loop() {
    unsigned long currentMillis = millis();

    // 1. Check Bluetooth Connection State
    bool isConnected = bleMouseIsConnected();

    // Log connection state transitions
    if (isConnected != s_lastConnected) {
        s_lastConnected = isConnected;
        #if DEBUG_SERIAL
        if (isConnected) {
            Serial.println("[STATUS] >>> Laptop Connected! Finger Mouse is Active. <<<");
        } else {
            Serial.println("[STATUS] >>> Laptop Disconnected! Waiting for connection... <<<");
        }
        #endif
    }

    // Maintain BLE advertising if disconnected
    if (!isConnected) {
        bleMouseCheckConnection();
    }

    // 2. Update Status LED
    updateStatusLED(isConnected);

    // 3. Dynamic Update Interval (Conserves power when not connected)
    unsigned long pollInterval = isConnected ? ACTIVE_UPDATE_INTERVAL_MS : IDLE_UPDATE_INTERVAL_MS;

    if (currentMillis - s_lastPollTime < pollInterval) {
        return; // Yield CPU until next scheduled update slot
    }
    s_lastPollTime = currentMillis;

    // 4. Update Tactile Buttons (Left and Right Click with Debounce)
    updateButton(s_btnLeft);
    updateButton(s_btnRight);

    // 5. Process Motion and Send Cursor Updates (Only when BLE is connected)
    if (isConnected) {
        float gx = 0.0f, gy = 0.0f, gz = 0.0f;
        float ax = 0.0f, ay = 0.0f, az = 0.0f;

        // Read calibrated sensor values
        if (s_sensor.readSensor(gx, gy, gz, ax, ay, az)) {
            int8_t deltaX = 0;
            int8_t deltaY = 0;

            // Apply dead-zone filter, EMA smoothing, sensitivity curve, and subpixel accumulation
            s_sensor.processMotion(gx, gy, gz, deltaX, deltaY);

            // Transmit relative mouse motion report if non-zero
            if (deltaX != 0 || deltaY != 0) {
                bleMouseMove(deltaX, deltaY);
            }
        }
    }
}
