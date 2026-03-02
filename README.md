# Standalone Passive Reconnaissance System

This project is a standalone passive reconnaissance system developed using PlatformIO. It involves an ESP32 acting as a master node and an ESP8266 (ESP-201) acting as a sniffer.

## Project Structure

The project is configured via `platformio.ini` with the following environments:

### ESP32 Master
- **Environment:** `esp32_master`
- **Platform:** `espressif32`
- **Board:** `esp32dev`
- **Framework:** `arduino`
- **Dependencies:** `NimBLE-Arduino` (Bluetooth Low Energy)

### ESP-201 Sniffer
- **Environment:** `esp201_sniffer`
- **Platform:** `espressif8266`
- **Board:** `esp01_1m`
- **Framework:** `arduino`

## Usage

1. Open the project in an IDE with PlatformIO support (e.g., VS Code).
2. Select the desired environment.
3. Build and upload to the target device.