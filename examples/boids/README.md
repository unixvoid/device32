# PlateScam Device
An esp32 based device used for sending mock dish data to the API. Less room and more repeatable test data.

## Hardware Specifications
- **Microcontroller**: Seeed XIAO ESP32C3
- **Display**: 128x64 OLED (SSD1306)
- **Storage**: SD Card for local image storage
- **Connectivity**: Wi-Fi and Bluetooth LE for provisioning
- **Input**: Single button for all interactions

## Device Features
- Dual-bank configuration system (Bank A / Bank B)
- Wi-Fi provisioning via Bluetooth LE
- Web-based configuration interface
- Random weight generation for uploaded dishes
- Base36 timestamp display

## Button Functions
The device uses a single button with different press durations to trigger various functions:

### Quick Tap (200ms+)
- **Normal Mode**: Upload image pair (dish + logo) to API with current bank configuration
- **Config Mode**: Exit configuration mode and return to normal operation

### Medium Hold (3-6 seconds)
**Bank Switch**
- Display shows "BANK SW" during hold
- Toggles between Bank A and Bank B configurations
- Selected bank name appears briefly after release

### Long Hold (6-10 seconds)
**Configuration Mode**
- Display shows "CONFIG" during hold and while active
- Starts local web server for device configuration
- Access configuration at device's IP address
- Allows editing of bank settings, API endpoints, credentials, and images

### Very Long Hold (10-14 seconds)
**Wi-Fi Reset**
- Display shows "RST WIFI" during hold
- Clears saved Wi-Fi credentials from device memory
- Device will enter provisioning mode on next restart
- Requires re-pairing with Wi-Fi network

### Terminal Hold (14+ seconds)
**Device Restart**
- Forces immediate device restart
- Use for emergency recovery or stuck states

## Device Operation

### Initial Setup
1. **Power on the device**
   - The device will show a unified boot screen with border
   - Initial message: "PlateScam Booting..." 
   - Device endpoint ID will appear at bottom of screen

2. **WiFi Connection Attempt**
   - Screen will show: "Connecting WiFi" with progress dots
   - If previously configured, device attempts automatic connection
   - If successful, proceeds to normal operation

3. **BLE Provisioning Mode (if no WiFi credentials)**
   - Screen shows: "BLE Provisioning" with endpoint ID
   - Device broadcasts BLE service named: `fooq-XXXXXX` (where XXXXXX is device MAC)

### WiFi Provisioning Steps
1. **Download the ESP BLE Prov App**
   - Android: Search "ESP BLE Prov" on Google Play Store
   - iOS: Search "ESP BLE Prov" on App Store

2. **Connect via App**
   - Open the ESP BLE Prov app
   - Scan for devices or look for device name `fooq-XXXXXX`
   - Select your device from the list

3. **Authenticate**
   - When prompted for Proof of Possession (PoP) PIN
   - Enter: `fooqai00`

4. **Configure WiFi**
   - Select your WiFi network from the list
   - Enter your WiFi password
   - Tap "Provision"

5. **Wait for Connection**
   - Device will attempt to connect
   - On success, device will reboot automatically
   - Device will show "Syncing Time..." then normal operation

### Normal Operation
- **Display**: Animated eyes with various states (open, blink, look left/right)
- **HUD**: Shows current timestamp in Base36 format
- **Status**: Current bank indicator and connection status
- **Image Upload**: Quick button tap uploads random image pair from SD card

### Bank System
The device supports two independent configuration banks:
- **Bank A** and **Bank B**: Each with separate API endpoints and credentials
- **Quick Switch**: Medium hold toggles between banks
- **Visual Feedback**: Bank name displayed after switching
- **Current Status**: Active bank shown in HUD

### Configuration Web Interface
When in config mode, access the web interface at the device's IP address:
- **Bank Settings**: Configure API URLs, keys, and endpoints for both banks
- **Image Pool Managment**: Add/Remove images from the image pool
- **Local OTA**: Upload binary file to flash device

## File Structure
The device expects photos+logos together in pairs to be placed in the `/image_pool/` directory of the SD card.  
Images should be in the following format `image.jpg` and the cooresponding logo `image-L.jpg`.  
   
The device will create an index of the files on first boot for faster downstream performance. The index is placed at the root of the SD card in a file named `image_index.txt`