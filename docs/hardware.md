# Hardware Documentation

## Device Overview

This project is designed for the **Seeed Xiao ESP32C3** microcontroller board. The ESP32C3 is a low-power, RISC-V-based SoC with Wi-Fi and Bluetooth capabilities, making it suitable for IoT applications.

## Display

The display used is a **SSD1315 128x64 OLED** screen. This is a monochrome OLED display with a resolution of 128x64 pixels, communicating via I2C protocol.

### Pin Mapping

The following table shows the pin connections for the hardware components:

| Component | Xiao ESP32C3 Pin | GPIO Number | Description |
|-----------|------------------|-------------|-------------|
| OLED SDA  | Pin 7           | GPIO5      | I2C Data line for the SSD1315 display |
| OLED SCL  | Pin 6           | GPIO4      | I2C Clock line for the SSD1315 display |
| Button    | Pin 5           | GPIO3      | Input pin for user button |

### Wiring Diagram

```
Seeed Xiao ESP32C3          SSD1315 OLED
-----------------          -------------
Pin 7 (GPIO5)    ------>   SDA
Pin 6 (GPIO4)    ------>   SCL
GND             ------>   GND
3.3V            ------>   VCC

Button
------
Pin 5 (GPIO3)   <------   Button signal
GND             <------   Button GND
```

### Notes
- The I2C address for the SSD1315 is 0x3C