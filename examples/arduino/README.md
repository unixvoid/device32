# Arduino Examples

This folder contains Arduino sketches for the device32 hardware.

How to use
1. Open Arduino IDE (or VS Code + Arduino extension).
2. Install ESP32 boards via Boards Manager (esp32 by Espressif).
3. Select board and upload the sketch:
   - File -> Open -> examples/arduino/basic_display/basic_display.ino
4. Wiring: see docs/hardware.md for pin mapping and screen wiring.

Included examples
- basic_display — a minimal demo that initializes the screen and draws simple UI.
- wifi_touch — shows Wi-Fi setup and a touch-based UI (if your board has touch capability).

Notes
- Add libraries via Sketch -> Include Library -> Manage Libraries as documented in each example's header.