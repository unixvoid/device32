# Starfield

Starfield is a simulator that constantly shows the navigation and progression through a field of stars. The stars move past the user as they progress forwards.

## Requirements
- device32 hardware (ESP32-based with OLED display)
- PlatformIO development environment
- USB connection for flashing

## Setup
1. Open this folder (`examples/starfield/`) in VSCode with PlatformIO installed.
2. Connect your device32 via USB.
3. Use PlatformIO to build and flash the project.

## Usage
Displays a starfield effect simulating movement through space on the OLED screen.

## Controls
- No user controls; the animation is fully automated.

## Notes
- Stars are drawn as moving points to create depth.