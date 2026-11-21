# Bluetooth Finger Air Mouse (ESP32-C3 Super Mini + MPU6050)

Complete, production-ready firmware and hardware engineering guide for a wearable **Bluetooth Low Energy (BLE) Finger Air Mouse** built with an **ESP32-C3 Super Mini**, **MPU6050 (GY-521)** 6-axis motion sensor, tactile click buttons, and a rechargeable 3.7V Li-Po battery power system.

---

## Table of Contents
1. [Project Overview](#project-overview)
2. [Hardware Components & Bill of Materials](#hardware-components--bill-of-materials)
3. [ESP32-C3 Super Mini Pinout & Hardware Validation](#esp32-c3-super-mini-pinout--hardware-validation)
4. [Complete Circuit Schematic & Wiring Guide](#complete-circuit-schematic--wiring-guide)
5. [Firmware Architecture & Features](#firmware-architecture--features)
6. [Step-by-Step Software Installation Guide](#step-by-step-software-installation-guide)
7. [Compiling & Uploading Firmware](#compiling--uploading-firmware)
8. [First-Time Calibration & Serial Diagnostics](#first-time-calibration--serial-diagnostics)
9. [Pairing with Windows Laptop](#pairing-with-windows-laptop)
10. [Testing & Fine-Tuning Motion Parameters](#testing--fine-tuning-motion-parameters)
11. [Troubleshooting Guide](#troubleshooting-guide)

---

## 1. Project Overview

The **Bluetooth Finger Air Mouse** transforms natural finger gestures into smooth, responsive laptop cursor movements. Worn comfortably on the index finger:
* **Pointing side-to-side (Yaw)** rotates the cursor horizontally across your display.
* **Tilting up-and-down (Pitch)** moves the cursor vertically.
* **Tactile Push Buttons** placed under your thumb handle Left Click, Right Click, Double-Click, and Click-and-Drag.
* **Native BLE HID:** Operates as a genuine Bluetooth Human Interface Device (HID) mouse on Windows, macOS, Linux, Android, and iOS without installing any custom drivers, software, or receiver USB dongles.

---

## 2. Hardware Components & Bill of Materials

| Item # | Component | Model / Spec | Purpose |
| :--- | :--- | :--- | :--- |
| **1** | Microcontroller | **ESP32-C3 Super Mini** | Single-core 32-bit RISC-V @ 160MHz, BLE 5.0, compact 21x18mm form factor |
| **2** | Motion Sensor (IMU) | **MPU6050 (GY-521)** | 6-axis motion tracking (3-axis gyro + 3-axis accel) with onboard DLPF |
| **3** | Battery | **3.7V Li-Po Battery (150–500mAh)** | Lightweight wearable rechargeable power source |
| **4** | Charger Module | **TP4056 Type-C with Protection** | Safe Li-Po charging with over-charge, over-discharge, & short-circuit protection |
| **5** | Voltage Regulator | **3.3V Buck-Boost DC-DC Regulator** | Provides stable 3.30V rail whether Li-Po is 4.2V (full) or 3.0V (empty) |
| **6** | Click Buttons | **2x Tactile Push Buttons (6x6mm)** | Left mouse click and Right mouse click |
| **7** | Power Switch | **Mini SPDT Slide Switch** | Physical ON/OFF battery isolation |
| **8** | Wiring | **Silicone 30 AWG Jumper Wires** | Flexible, lightweight wiring between finger mount and controller |

---

## 3. ESP32-C3 Super Mini Pinout & Hardware Validation

The **ESP32-C3 Super Mini** is an ultra-compact development board. Below is the validation of the GPIO assignments used in this project:

```
                      +-------------------+
                      |   [USB-C PORT]    |
               5V  ---| 1              16 |--- GPIO 21 (U0TXD)
              GND  ---| 2              15 |--- GPIO 20 (U0RXD)
             3.3V  ---| 3              14 |--- GPIO 10
           GPIO 0  ---| 4              13 |--- GPIO 9 (BOOT Button / Strapping)
           GPIO 1  ---| 5              12 |--- GPIO 8 (Onboard Blue LED / Strapping)
           GPIO 2  ---| 6              11 |--- GPIO 7 (Right Click Button)
           GPIO 3  ---| 7              10 |--- GPIO 6 (Left Click Button)
 (I2C SDA) GPIO 4  ---| 8               9 |--- GPIO 5 (I2C SCL)
                      +-------------------+
```

### Pin Verification Analysis:
* **GPIO 4 (SDA) & GPIO 5 (SCL):** These are general-purpose I/O pins (ADC1_CH4 and ADC2_CH0) with no strapping conflicts. They provide clean hardware I2C communication with the MPU6050.
* **GPIO 6 (Left Click) & GPIO 7 (Right Click):** On classic ESP32 chips, GPIO 6–11 are connected to external SPI flash. **On ESP32-C3, internal flash uses GPIO 12–17.** Therefore, GPIO 6 and GPIO 7 are broken out directly to the headers on the Super Mini and operate safely as standard digital inputs with internal pull-up resistors (`INPUT_PULLUP`).
* **GPIO 8 (Onboard Blue LED):** Connected on the Super Mini PCB to the onboard status LED (active LOW). We utilize GPIO 8 exclusively as an output to give user feedback (flashing = calibrating/pairing, solid = connected). Note: GPIO 8 is a strapping pin; keeping it as an output and not hard-grounding it externally guarantees normal boot behavior.
* **GPIO 9 (BOOT button):** Left unassigned so the physical bootloader button operates without interference.

---

## 4. Complete Circuit Schematic & Wiring Guide

### 4.1 MPU6050 (GY-521) to ESP32-C3 Super Mini

| MPU6050 Pin | ESP32-C3 Super Mini Pin | Function / Description |
| :--- | :--- | :--- |
| **VCC** | **3.3V** | Power rail (3.3V regulated) |
| **GND** | **GND** | Common Ground |
| **SDA** | **GPIO 4** | I2C Data line |
| **SCL** | **GPIO 5** | I2C Clock line |
| **AD0** | **GND** | Sets 7-bit I2C Address to `0x68` (do not leave floating) |
| **INT** | *Not Connected* | Unused |

### 4.2 Push Buttons to ESP32-C3 Super Mini

| Push Button | Terminal 1 (Signal) | Terminal 2 (Ground) | Mode in Firmware |
| :--- | :--- | :--- | :--- |
| **Left Click Button** | **GPIO 6** | **GND** | `INPUT_PULLUP` (Active LOW when pressed) |
| **Right Click Button** | **GPIO 7** | **GND** | `INPUT_PULLUP` (Active LOW when pressed) |

### 4.3 Power System Wiring (Li-Po + TP4056 + Buck-Boost)

```
[3.7V Li-Po Battery]
     (+) -------> B+ [TP4056 Charger]
     (-) -------> B- [TP4056 Charger]

[TP4056 Module]
     OUT(-) ---------------------------> GND Common (ESP32-C3 GND, MPU6050 GND, Buttons GND)
     OUT(+) ----> [SPDT Slide Switch] -> VIN [3.3V Buck-Boost Regulator]

[3.3V Buck-Boost Regulator]
     VOUT (3.3V) ----------------------> ESP32-C3 3.3V Pin & MPU6050 VCC Pin
     GND ------------------------------> Common GND
```

> **Why a 3.3V Buck-Boost Regulator?**  
> A standard 3.7V Li-Po battery voltage drops from 4.20V (full) down to 3.00V (discharged). A buck-boost regulator steps down when the battery is >3.3V and steps up when it drops below 3.3V, ensuring the ESP32-C3 and MPU6050 receive a rock-solid 3.30V rail throughout the entire battery discharge cycle, maximizing battery run-time.

---

## 5. Firmware Architecture & Features

The project is structured into modular, clean C++ files:

```
finger-air-mouse/
├── finger_mouse.ino     # Main entry point, setup(), loop(), non-blocking scheduling
├── config.h             # Pinout, sensitivity, deadzone, smoothing, and BLE settings
├── sensor.h             # MPU6050 class declarations and hardware register definitions
├── sensor.cpp           # Direct I2C driver, calibration, EMA filter, motion mapping
├── mouse.h              # BLE HID Mouse API declarations
├── mouse.cpp            # Lightweight NimBLE-Arduino HID Mouse implementation
└── README.md            # Comprehensive project documentation
```

### Key Firmware Capabilities:
1. **Direct I2C Register Driver (`Wire.h`):** Interfaces directly with MPU6050 registers (0x1A DLPF, 0x1B Gyro Range, 0x6B Power Management, 0x75 WHO_AM_I) without bloated third-party dependencies.
2. **Hardware Digital Low Pass Filter (DLPF):** Sets MPU6050 internal bandwidth to ~44 Hz, physically rejecting hand vibrations and motor noise.
3. **Startup Gyroscope Zero-Rate Calibration:** Averages 400 stationary samples at boot to measure sensor baseline drift and subtracts offsets from every future reading.
4. **Dead-Band Noise Gate:** Filters out angular velocities below `DEADZONE_THRESHOLD_DPS` (1.5°/s), stopping mouse cursor crawling when resting.
5. **Exponential Moving Average (EMA) Smoothing:** Low-pass filter removes tremors while maintaining sharp tracking response.
6. **Dynamic Velocity Acceleration:** Small rotations yield pixel-level precision, while quick flicks accelerate across multi-monitor setups.
7. **Fractional Sub-Pixel Accumulator:** Preserves sub-integer cursor movement increments, preventing slow motions from truncating to zero.
8. **Robust Button Debouncing:** 35ms software debounce filter supports single-click, double-click, and click-and-drag.
9. **NimBLE-Arduino BLE HID Engine:** Consumes 50% less RAM than Bluedroid, boots instantly, broadcasts standard mouse reports, and automatically restarts advertising if disconnected.
10. **Battery-Saving Timing:** Runs at 100 Hz (10ms) when connected, but throttles down to 4 Hz (250ms) when disconnected.

---

## 6. Step-by-Step Software Installation Guide

### Step 1: Install Arduino IDE
Download and install **Arduino IDE 2.3+** from the official site:  
[https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)

### Step 2: Add Espressif ESP32 Board Package URL
1. Open Arduino IDE.
2. Go to **File > Preferences** (or `Ctrl + ,`).
3. Locate **Additional boards manager URLs**.
4. Paste the following URL:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
5. Click **OK**.

### Step 3: Install the ESP32 Board Core
1. Open **Boards Manager** from the left sidebar icon (or `Ctrl + Shift + B`).
2. Search for **`esp32`**.
3. Locate **esp32 by Espressif Systems** and click **Install**.

### Step 4: Install the NimBLE-Arduino Library
1. Open **Library Manager** from the left sidebar icon (or `Ctrl + Shift + I`).
2. Search for **`NimBLE-Arduino`**.
3. Locate **NimBLE-Arduino** by **h2zero**.
4. Click **Install**.

---

## 7. Compiling & Uploading Firmware

### Step 1: Open the Project
Open `finger_mouse/finger_mouse.ino` (or `finger_mouse.ino`) in Arduino IDE.

### Step 2: Select Board and Port
1. In the top toolbar, open the Board dropdown and select **Select other board and port...**
2. Search for: **`ESP32C3 Dev Module`** (or **`ESP32-C3 Super Mini`**).
3. Select your ESP32-C3's active **COM Port** (e.g. `COM3`, `COM5`).

### Step 3: Configure Tools Settings
Under the **Tools** menu, ensure these settings are selected:
* **Board:** `ESP32C3 Dev Module`
* **USB CDC On Boot:** `Enabled` *(Crucial for Serial Monitor debugging over native USB!)*
* **CPU Frequency:** `160MHz (WiFi/BT)`
* **Flash Frequency:** `80MHz`
* **Flash Mode:** `QIO`
* **Flash Size:** `4MB (32Mb)`
* **Partition Scheme:** `Default 4MB with spiffs`
* **Core Debug Level:** `None` (or `Info` during initial testing)
* **Upload Speed:** `921600` (or `115200` if connection is unstable)

### Step 4: Compile and Upload
1. Click the **Verify** checkmark button (`Ctrl + R`) to compile the sketch.
2. Click the **Upload** arrow button (`Ctrl + U`) to flash the firmware.

> **Flashing Tip (Manual Bootloader Mode):**  
> If the IDE shows `A fatal error occurred: Failed to connect to ESP32-C3`:
> 1. Press and **HOLD** the physical **BOOT** button on the Super Mini.
> 2. Press and release the **RST** button.
> 3. **Release** the **BOOT** button.
> 4. Click **Upload** again in Arduino IDE.

---

## 8. First-Time Calibration & Serial Diagnostics

1. Plug your ESP32-C3 Super Mini into the PC via USB-C.
2. Open **Serial Monitor** in Arduino IDE (`Ctrl + Shift + M`).
3. Set the baud rate in the bottom-right corner to **`115200 baud`**.
4. Press the **RST** button on the ESP32-C3 board.
5. **KEEP THE SENSOR COMPLETELY STILL ON A FLAT DESK FOR 2 SECONDS.**
6. You will see output similar to:

```
==================================================
         BLUETOOTH FINGER AIR MOUSE               
      ESP32-C3 Super Mini + MPU6050 (GY-521)      
==================================================
[INFO] CPU Frequency: 160 MHz
[INFO] Left Button:  GPIO 6 (INPUT_PULLUP)
[INFO] Right Button: GPIO 7 (INPUT_PULLUP)
[INFO] I2C SDA:      GPIO 4
[INFO] I2C SCL:      GPIO 5
[INFO] Status LED:   GPIO 8
[INIT] Initializing MPU6050 sensor...
[MPU6050] Successfully detected sensor (WHO_AM_I = 0x68)
[MPU6050] Hardware registers configured successfully.
[CALIBRATION] Starting Gyroscope Calibration (400 samples)...
[CALIBRATION] >> IMPORTANT: Keep your finger / device completely still! <<
[CALIBRATION] Calibration Complete!
[CALIBRATION] Offsets (deg/s) -> X: -1.24 | Y: 0.85 | Z: -0.42 (from 400 samples)
[INIT] Initializing BLE HID Mouse service...
[BLE] Initializing NimBLE HID Device: 'Finger Mouse'...
[BLE] Advertising started. Waiting for laptop pairing...
==================================================
[READY] Finger Mouse is broadcasting as: 'Finger Mouse'
[READY] Open your laptop Bluetooth settings to pair.
==================================================
```

---

## 9. Pairing with Windows Laptop

1. On your Windows laptop, open **Settings** (`Win + I`).
2. Navigate to **Bluetooth & devices**.
3. Toggle Bluetooth to **ON**.
4. Click **Add device** and select **Bluetooth**.
5. Wait a few seconds for devices to populate. Look for **Finger Mouse** (with a mouse icon).
6. Click **Finger Mouse**.
7. Windows will pair and display: **"Your device is ready to go!"**
8. Check the Serial Monitor:
   ```
   [BLE] Client connected!
   [STATUS] >>> Laptop Connected! Finger Mouse is Active. <<<
   ```
9. The onboard blue LED will switch from slow blinking to **solid ON**, indicating an active connection.

---

## 10. Testing & Fine-Tuning Motion Parameters

### 10.1 Testing Motions & Clicks
* **Horizontal Movement:** Rotate your index finger left and right. The cursor should glide horizontally across the screen.
* **Vertical Movement:** Tilt your finger up and down. The cursor should move up and down.
* **Left Click:** Press the button on GPIO 6. The Serial Monitor will report `[BUTTON] Left Click: PRESSED` and the PC will register a standard left click.
* **Right Click:** Press the button on GPIO 7. A context menu will appear on your desktop.
* **Click-and-Drag:** Hold down the Left Click button and tilt your finger. You can highlight text or drag desktop icons smoothly.

### 10.2 Customizing Settings in `config.h`

All operational parameters are centralized in `config.h`:

| Setting | Default | Description & Customization Guide |
| :--- | :--- | :--- |
| `INVERT_X` | `false` | Change to `true` if horizontal movement feels reversed. |
| `INVERT_Y` | `true` | Change to `false` if tilting down moves the cursor up instead of down. |
| `AXIS_MAP_X` | `AXIS_Z` | If mounted vertically on the side of a finger, swap with `AXIS_X` or `AXIS_Y`. |
| `AXIS_MAP_Y` | `AXIS_Y` | Selects which gyro axis maps to cursor elevation. |
| `SENSITIVITY_X` | `2.2f` | Increase (e.g. `3.0f`) for faster cursor movement, decrease (e.g. `1.5f`) for slower. |
| `SENSITIVITY_Y` | `2.2f` | Vertical sensitivity multiplier. |
| `DEADZONE_THRESHOLD_DPS` | `1.5f` | Increase (e.g. `2.5f`) if cursor slowly crawls when holding still. |
| `SMOOTHING_FACTOR` | `0.35f` | Lower value (e.g. `0.20f`) = smoother cursor; higher (e.g. `0.60f`) = more responsive. |
| `ENABLE_ACCELERATION` | `true` | Enables velocity curve: slow tilts give 1-pixel precision; fast flicks cross 4K screens. |
| `ACCEL_EXPONENT` | `1.22f` | Curve steepness for dynamic acceleration. |
| `BUTTON_DEBOUNCE_MS` | `35` | Debounce duration (ms). Increase if switch exhibits phantom double-clicks. |

---

## 11. Troubleshooting Guide

### Issue 1: `A fatal error occurred: Failed to connect to ESP32-C3`
* **Cause:** The ESP32-C3 USB-Serial JTAG controller is in execution mode rather than download mode.
* **Solution:** Hold the **BOOT** button, press **RST**, release **BOOT**, and press Upload in Arduino IDE. Also verify USB cable has data lines (not a charge-only cable).

### Issue 2: `[CRITICAL ERROR] Failed to communicate with MPU6050!`
* **Cause:** Loose jumper wires, incorrect I2C pins, or bad power supply.
* **Solution:**
  1. Verify SDA connects to **GPIO 4** and SCL connects to **GPIO 5**.
  2. Verify MPU6050 **AD0 is tied to GND** (sets address `0x68`).
  3. Ensure MPU6050 VCC receives a solid 3.3V.
  4. Test if MPU6050 responds at `0x69` by changing `MPU6050_I2C_ADDR` to `0x69` in `config.h`.

### Issue 3: Cursor drifts slowly when hand is stationary
* **Cause:** Gyro zero-rate bias offset or resting hand tremor.
* **Solution:**
  1. Restart the device with the finger mouse sitting flat on a table so startup calibration captures true zero.
  2. Increase `DEADZONE_THRESHOLD_DPS` in `config.h` from `1.5f` to `2.2f` or `2.8f`.

### Issue 4: Cursor movement feels jittery or shaky
* **Cause:** Hand micro-tremors amplified by high sensitivity.
* **Solution:**
  1. Decrease `SMOOTHING_FACTOR` in `config.h` to `0.25f` or `0.20f` (increases low-pass filtering).
  2. Ensure `MPU6050_DLPF_CFG` is set to `3` (~44 Hz filter).

### Issue 5: Bluetooth pairs but cursor does not move
* **Cause:** Host OS paired before HID services were fully indexed, or cached Bluetooth profile is stale.
* **Solution:**
  1. In Windows, go to **Settings > Bluetooth & devices > Devices**, locate **Finger Mouse**, click the three dots, and select **Remove device**.
  2. Power cycle the Finger Mouse.
  3. Re-scan and pair fresh.

### Issue 6: Device does not turn on from battery
* **Cause:** TP4056 protection circuit is in cutoff, or battery voltage is below 3.0V.
* **Solution:** Plug a USB-C charging cable into the TP4056 module for 5 seconds to activate the battery protection IC (DW01A).

---

## License

This project is released under the **MIT License**. Free for educational, maker, and commercial development.
