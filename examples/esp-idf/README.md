# ESP-IDF Examples

Native ESP-IDF projects for users who want full control and performance.

How to use
1. Install ESP-IDF (follow official docs).
2. Open the example folder (e.g. examples/esp-idf/examples/basic_display) and run:
   idf.py set-target esp32
   idf.py menuconfig      # optionally adjust sdkconfig
   idf.py build
   idf.py flash -p /dev/ttyUSB0

Notes
- Examples include sdkconfig.defaults where appropriate.
- Use components/ for reusable modules.