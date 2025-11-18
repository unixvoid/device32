# Game of Life

This is an example demo of Conway's Game of Life:
- Any live cell with fewer than two live neighbours dies, as if by underpopulation.
- Any live cell with two or three live neighbours lives on to the next generation.
- Any live cell with more than three live neighbours dies, as if by overpopulation.
- Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction.

## Requirements
- device32 hardware (ESP32-based with OLED display)
- PlatformIO development environment
- USB connection for flashing

## Setup
1. Open this folder (`examples/game_of_life/`) in VSCode with PlatformIO installed.
2. Connect your device32 via USB.
3. Use PlatformIO to build and flash the project.

## Usage
The simulation runs Conway's Game of Life, displaying evolving patterns of cells on the OLED screen.

## Controls
- No user controls; the evolution is fully automated.

## Notes
- Starts with a random initial configuration.
- The grid is scaled to fit the 128x64 OLED display.