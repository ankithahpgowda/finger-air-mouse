/**
 * =========================================================================
 * Project: Bluetooth Finger Mouse (ESP32-C3 Super Mini + MPU6050)
 * File:    mouse.h
 * Description: Declarations for Bluetooth Low Energy (BLE) Human Interface
 *              Device (HID) mouse implementation for ESP32-C3.
 *              Supports button clicking, dragging, scrolling, and cursor motion.
 * =========================================================================
 */

#ifndef MOUSE_H
#define MOUSE_H

#include <Arduino.h>
#include "config.h"

// Standard Mouse Button Masks
#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

/**
 * @brief Initializes the Bluetooth Low Energy (BLE) HID Mouse service.
 * @param deviceName Name advertised to laptops / phones (e.g. "Finger Mouse")
 * @param manufacturer Manufacturer string (e.g. "DIY Embedded")
 * @param batteryLevel Initial battery percentage (0-100)
 * @return true on successful BLE initialization
 */
bool bleMouseInit(const char* deviceName = BLE_DEVICE_NAME, 
                  const char* manufacturer = BLE_MANUFACTURER, 
                  uint8_t batteryLevel = BLE_BATTERY_LEVEL);

/**
 * @brief Checks if a Bluetooth host (laptop/PC) is paired and actively connected.
 * @return true if connected
 */
bool bleMouseIsConnected();

/**
 * @brief Sends relative cursor movement report over BLE.
 * @param x Horizontal relative displacement (-127 to +127)
 * @param y Vertical relative displacement (-127 to +127)
 * @param wheel Scroll wheel movement (-127 to +127, default 0)
 */
void bleMouseMove(int8_t x, int8_t y, int8_t wheel = 0);

/**
 * @brief Holds down one or more mouse buttons (for clicking or dragging).
 * @param button Button mask (MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, etc.)
 */
void bleMousePress(uint8_t button);

/**
 * @brief Releases one or more mouse buttons.
 * @param button Button mask (MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, etc.)
 */
void bleMouseRelease(uint8_t button);

/**
 * @brief Performs a full click (press + 15ms hold + release).
 * @param button Button mask (MOUSE_BUTTON_LEFT or MOUSE_BUTTON_RIGHT)
 */
void bleMouseClick(uint8_t button);

/**
 * @brief Checks if a given mouse button is currently held down.
 * @param button Button mask to query
 * @return true if currently pressed
 */
bool bleMouseIsPressed(uint8_t button);

/**
 * @brief Updates battery level reported to host operating system.
 * @param level Battery percentage (0-100)
 */
void bleMouseSetBattery(uint8_t level);

/**
 * @brief Periodic maintenance call. Ensures BLE advertising restarts
 *        if host disconnected.
 */
void bleMouseCheckConnection();

#endif // MOUSE_H
