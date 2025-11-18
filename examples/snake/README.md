# Snake

Simulation of the simple snake game. Snake starts out as 1 segment and hunts food pellets. When the snake consumes a pellet it increases in length by 1 segment. The game is over when the snake runs into a wall or itself.

## Requirements
- device32 hardware (ESP32-based with OLED display)
- PlatformIO development environment
- USB connection for flashing

## Setup
1. Open this folder (`examples/snake/`) in VSCode with PlatformIO installed.
2. Connect your device32 via USB.
3. Use PlatformIO to build and flash the project.

## Usage
An AI-controlled snake plays the game automatically, seeking food and avoiding obstacles.

## Controls
- No user controls; the snake is controlled by AI pathfinding.

## Notes
- Uses BFS algorithm for optimal pathfinding to food.
- Game resets automatically on game over.