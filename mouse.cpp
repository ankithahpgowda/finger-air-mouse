/**
 * =========================================================================
 * Project: Bluetooth Finger Mouse (ESP32-C3 Super Mini + MPU6050)
 * File:    mouse.cpp
 * Description: Implementation of Bluetooth Low Energy (BLE) Human Interface
 *              Device (HID) mouse service for ESP32-C3 using NimBLE.
 * =========================================================================
 */

#include "mouse.h"

#if USE_EXTERNAL_BLE_MOUSE_LIB

// -------------------------------------------------------------------------
// Option B: External BleMouse Library Backend
// -------------------------------------------------------------------------
#define USE_NIMBLE
#include <BleMouse.h>

static BleMouse s_bleMouse(BLE_DEVICE_NAME, BLE_MANUFACTURER, BLE_BATTERY_LEVEL);

bool bleMouseInit(const char* deviceName, const char* manufacturer, uint8_t batteryLevel) {
    (void)deviceName;
    (void)manufacturer;
    (void)batteryLevel;
    s_bleMouse.begin();
    return true;
}

bool bleMouseIsConnected() {
    return s_bleMouse.isConnected();
}

void bleMouseMove(int8_t x, int8_t y, int8_t wheel) {
    if (s_bleMouse.isConnected()) {
        s_bleMouse.move(x, y, wheel);
    }
}

void bleMousePress(uint8_t button) {
    if (s_bleMouse.isConnected()) {
        s_bleMouse.press(button);
    }
}

void bleMouseRelease(uint8_t button) {
    if (s_bleMouse.isConnected()) {
        s_bleMouse.release(button);
    }
}

void bleMouseClick(uint8_t button) {
    if (s_bleMouse.isConnected()) {
        s_bleMouse.click(button);
    }
}

bool bleMouseIsPressed(uint8_t button) {
    return s_bleMouse.isPressed(button);
}

void bleMouseSetBattery(uint8_t level) {
    s_bleMouse.setBatteryLevel(level);
}

void bleMouseCheckConnection() {
    // Handled internally by BleMouse library
}

#else

// -------------------------------------------------------------------------
// Option A: Native, Lightweight NimBLE HID Mouse Backend (Recommended)
// -------------------------------------------------------------------------
// Requires ONLY the standard "NimBLE-Arduino" library by h2zero.
// Fully compatible with ESP32-C3 RISC-V and ESP32 Core v2.x and v3.x!
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLEHIDDevice.h>

#if __has_include("NimBLECppVersion.h")
#include "NimBLECppVersion.h"
#endif

// Standard USB / BLE HID Mouse Report Descriptor
// Report format: [Buttons (1 byte), X (int8), Y (int8), Wheel (int8)]
static const uint8_t s_mouseReportMap[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop Controls)
    0x09, 0x02,                    // USAGE (Mouse)
    0xA1, 0x01,                    // COLLECTION (Application)
    0x85, 0x01,                    //   REPORT_ID (1)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xA1, 0x00,                    //   COLLECTION (Physical)

    // 3 Buttons: Left (bit 0), Right (bit 1), Middle (bit 2)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data, Variable, Absolute)

    // 5-bit padding to align report to byte boundary
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Constant, Variable, Absolute)

    // Relative X and Y Axis Motion (-127 to +127)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop Controls)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data, Variable, Relative)

    // Relative Scroll Wheel (-127 to +127)
    0x09, 0x38,                    //     USAGE (Wheel)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x81, 0x06,                    //     INPUT (Data, Variable, Relative)

    0xC0,                          //   END_COLLECTION (Physical)
    0xC0                           // END_COLLECTION (Application)
};

static NimBLEServer*         s_pServer    = nullptr;
static NimBLEHIDDevice*       s_pHidDevice = nullptr;
static NimBLECharacteristic*  s_inputMouse = nullptr;
static uint8_t                s_buttonMask = 0;
static bool                   s_connected  = false;

// Version-compatible NimBLE Connection Callback Handler
#if defined(NIMBLE_CPP_VERSION_MAJOR) && (NIMBLE_CPP_VERSION_MAJOR >= 2)
class MouseServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        s_connected = true;
        #if DEBUG_SERIAL
        Serial.println("[BLE] Client connected! Address: " + String(connInfo.getAddress().toString().c_str()));
        #endif
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        s_connected = false;
        #if DEBUG_SERIAL
        Serial.printf("[BLE] Client disconnected (reason %d). Restarting advertising...\n", reason);
        #endif
        NimBLEDevice::startAdvertising();
    }
};
#else
class MouseServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        s_connected = true;
        #if DEBUG_SERIAL
        Serial.println("[BLE] Client connected!");
        #endif
    }

    void onDisconnect(NimBLEServer* pServer) override {
        s_connected = false;
        #if DEBUG_SERIAL
        Serial.println("[BLE] Client disconnected. Restarting advertising...");
        #endif
        NimBLEDevice::startAdvertising();
    }
};
#endif

static void sendReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
    if (!bleMouseIsConnected() || s_inputMouse == nullptr) return;

    uint8_t report[4];
    report[0] = buttons & 0x07;
    report[1] = (uint8_t)x;
    report[2] = (uint8_t)y;
    report[3] = (uint8_t)wheel;

    s_inputMouse->setValue(report, sizeof(report));
    s_inputMouse->notify();
}

bool bleMouseInit(const char* deviceName, const char* manufacturer, uint8_t batteryLevel) {
    #if DEBUG_SERIAL
    Serial.printf("[BLE] Initializing NimBLE HID Device: '%s'...\n", deviceName);
    #endif

    // 1. Initialize BLE Stack
    NimBLEDevice::init(deviceName);

    // Configure security for HID pairing (Bonding enabled, No PIN/passkey required for mouse)
    NimBLEDevice::setSecurityAuth(true, false, false);

    // 2. Create BLE GATT Server
    s_pServer = NimBLEDevice::createServer();
    s_pServer->setCallbacks(new MouseServerCallbacks());

    // 3. Create Human Interface Device (HID) Service
    s_pHidDevice = new NimBLEHIDDevice(s_pServer);
    s_inputMouse = s_pHidDevice->getInputReport(1); // Report ID 1

    s_pHidDevice->setManufacturer(manufacturer);
    s_pHidDevice->setPnp(0x02, 0x045E, 0x0040, 0x0100); // Standard PnP ID (Microsoft Corp Mouse)
    s_pHidDevice->setHidInfo(0x00, 0x01); // Country code 0x00, Normal mode flags
    s_pHidDevice->setBatteryLevel(batteryLevel);

    // Load HID Report Descriptor Map
    s_pHidDevice->setReportMap((uint8_t*)s_mouseReportMap, sizeof(s_mouseReportMap));

    // Start HID services
    #if !defined(NIMBLE_CPP_VERSION_MAJOR) || (NIMBLE_CPP_VERSION_MAJOR < 2)
    s_pHidDevice->startServices();
    #endif
    s_pServer->start();

    // 4. Configure BLE Advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setAppearance(0x03C2); // HID_MOUSE icon in OS Bluetooth scan
    pAdvertising->addServiceUUID(s_pHidDevice->getHidService()->getUUID());
    pAdvertising->start();

    #if DEBUG_SERIAL
    Serial.println("[BLE] Advertising started. Waiting for laptop pairing...");
    #endif

    return true;
}

bool bleMouseIsConnected() {
    if (s_pServer == nullptr) return false;
    return (s_pServer->getConnectedCount() > 0);
}

void bleMouseMove(int8_t x, int8_t y, int8_t wheel) {
    if (x != 0 || y != 0 || wheel != 0) {
        sendReport(s_buttonMask, x, y, wheel);
    }
}

void bleMousePress(uint8_t button) {
    s_buttonMask |= button;
    sendReport(s_buttonMask, 0, 0, 0);
}

void bleMouseRelease(uint8_t button) {
    s_buttonMask &= ~button;
    sendReport(s_buttonMask, 0, 0, 0);
}

void bleMouseClick(uint8_t button) {
    bleMousePress(button);
    delay(15);
    bleMouseRelease(button);
}

bool bleMouseIsPressed(uint8_t button) {
    return (s_buttonMask & button) != 0;
}

void bleMouseSetBattery(uint8_t level) {
    if (s_pHidDevice != nullptr) {
        s_pHidDevice->setBatteryLevel(level);
    }
}

void bleMouseCheckConnection() {
    if (s_pServer != nullptr) {
        if (s_pServer->getConnectedCount() == 0) {
            NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
            if (pAdv != nullptr && !pAdv->isAdvertising()) {
                #if DEBUG_SERIAL
                Serial.println("[BLE] Advertising restarted.");
                #endif
                pAdv->start();
            }
        }
    }
}

#endif // !USE_EXTERNAL_BLE_MOUSE_LIB
