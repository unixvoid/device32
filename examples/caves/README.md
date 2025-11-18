# Caves

![Caves Demo](../../docs/gifs/caves.gif)

Caves is a nethack dungeon generator. The program generates random stronghold rooms and caves and draws it to the screen.

## Requirements
- device32 hardware (ESP32-based with OLED display)
- PlatformIO development environment
- USB connection for flashing

## Setup
1. Open this folder (`examples/caves/`) in VSCode with PlatformIO installed.
2. Connect your device32 via USB.
3. Use PlatformIO to build and flash the project.

## Usage
The generator runs continuously, displaying procedurally generated cave and room layouts on the OLED screen.

## Controls
- No user controls; the generation is fully automated.

## Notes
- The display shows a top-down view of the generated dungeon.