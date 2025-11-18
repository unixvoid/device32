# Boids

![Boids Demo](../../docs/gifs/boids.gif)

Boids is an artificial life program, developed by Craig Reynolds in 1986, which simulates the flocking behaviour of birds, and related group motion.


## Requirements
- device32 hardware (ESP32-based with OLED display)
- PlatformIO development environment
- USB connection for flashing

## Setup
1. Open this folder (`examples/boids/`) in VSCode with PlatformIO installed.
2. Connect your device32 via USB.
3. Use PlatformIO to build and flash the project.

## Usage
The simulation runs automatically, displaying a flock of boids on the OLED screen. The boids exhibit realistic flocking behavior through separation, alignment, and cohesion rules.

## Controls
- Press the button to reset the simulation with new random starting positions.

## Notes
- Parameters can be adjusted in `src/main.cpp` for tuning the simulation.