# Timer App
This is a configurable timer app for the device32 (ESP32-based board with OLED display and button). It features WiFi-based configuration through a captive portal and button controls for timer operation.

## Requirements
- device32 hardware (ESP32-based with OLED display and button)
- PlatformIO development environment
- USB connection for flashing

## Configuration
Edit the constants at the top of `src/main.cpp` to customize:

- **Default Timer Duration**:
  ```cpp
  #define DEFAULT_TIMER_SECONDS 5
  ```

- **WiFi AP Credentials** (optional):
  ```cpp
  const char* ap_ssid = "Timer Config";
  const char* ap_pass = "";
  ```

## Setup
1. Open this folder (`examples/timer/`) in VSCode with PlatformIO installed.
2. (Optional) Update the default timer duration in `src/main.cpp`.
3. Connect your device32 via USB.
4. Use PlatformIO to build and upload the project (`pio run --target upload`).

## Usage

### Button Controls
- **Tap button**: Start/Pause timer (or reset when finished)
- **Hold button (1 sec)**: Reset timer immediately

### WiFi Configuration
1. Connect to the "Timer Config" WiFi network
2. Your device will automatically open the configuration page (captive portal)
3. Set the timer duration in seconds (1 second to 24 hours)
4. Settings are saved to flash memory and persist across reboots

### Display
- Shows remaining time in readable format (e.g., "1h 30m 45s" or "5s")
- Timer state indicator (Ready/Running/Paused/Done!)
- When timer finishes, "0s" blinks until button is pressed

## Notes
- Timer duration is stored in flash memory using Preferences
- The device creates its own WiFi Access Point for configuration
- No internet connection required
- Captive portal makes configuration easy on any device
- Low power consumption, suitable for continuous operation