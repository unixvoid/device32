# Demo

Cycles through snake, brick_break, lava_lamp, boids, caves, morph, and starfield demos with manual and automatic controls.

## Requirements
- device32 hardware (ESP32-based with OLED display)
- PlatformIO development environment
- USB connection for flashing

## Setup
1. Open this folder (`examples/full_demo/`) in VSCode with PlatformIO installed.
2. Connect your device32 via USB.
3. Use PlatformIO to build and flash the project.

## Usage
The demo starts in auto-play mode and cycles through all animations/games automatically. Use the button to control playback.

## Controls
- **Tap the button**: Advance to the next demo and pause auto-play. The device will stay on that demo.
- **Hold the button (3+ seconds)**: Toggle auto-play mode on/off. When auto-play is on, demos cycle every 2 minutes.

## Notes
- Auto-play is enabled by default on startup.
- Each demo runs for 2 minutes before switching (when auto-play is enabled).