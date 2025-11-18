# device32
![device32](docs/device32_banner.png)

This repository contains PlatformIO example projects and documentation for device32, an ESP32-based device with OLED display and select button on the back.

Quick links
- examples/ — PlatformIO projects with individual READMEs
- docs/ — hardware, wiring, BOM, and enclosure notes

Getting started
1. **Install Visual Studio Code (VSCode)**: Download and install VSCode from [https://code.visualstudio.com/](https://code.visualstudio.com/) if you haven't already.
2. **Install the PlatformIO Extension**: Open VSCode, go to the Extensions view (Ctrl+Shift+X), search for "PlatformIO IDE", and install it. This extension provides the PlatformIO interface within VSCode.
3. **Install Prerequisites**:
   - Ensure Python 3.6+ is installed (PlatformIO requires it). Download from [https://www.python.org/](https://www.python.org/) if needed.
4. **Clone the Repository**:
   ```
   git clone https://github.com/unixvoid/device32.git
   ```
5. **Open the Project in VSCode**: Launch VSCode, select "File > Open Folder", and open the cloned `device32` directory.
6. **Run an Example**:
   - Each example in `examples/` is a separate PlatformIO project (with its own `platformio.ini`).
   - Open an example folder (e.g., `examples/boids/`) in VSCode as a new window or via "File > Open Folder".
   - Use the PlatformIO toolbar in VSCode to build (hammer icon) and flash (arrow icon) the project to your device32.
   - Connect your device32 via USB and select the correct serial port in PlatformIO if prompted.
7. **Additional Notes**:
   - Check the `docs/` folder for hardware setup, wiring, and BOM details.
   - If an example lacks a README, refer to the main repo README or PlatformIO docs for general ESP32 flashing guidance.
   - For issues, ensure your device32 is powered on and connected, and check the PlatformIO logs in VSCode's terminal.

Contributions
See CONTRIBUTING.md for contribution guidelines and example creation guidance.

License
This repo is licensed under the MIT License — see LICENSE.