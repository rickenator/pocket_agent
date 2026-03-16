# WiFi Provisioning Guide

Complete guide for setting up WiFi on your Smart Agent device.

## Overview

The Smart Agent uses **IoT-style WiFi provisioning** for simple, secure network configuration. Instead of hardcoding credentials, the device creates a temporary access point that allows you to configure WiFi settings through a web interface.

## How It Works

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│   Device    │  AP Mode │   Your Phone  │  Config  │   Device    │
│  (Boots)    │ ───────→ │  (Connects)   │ ───────→ │ (Connects)  │
│ Buddy-XXXX  │         │ 192.168.4.1   │          │  Your WiFi  │
└─────────────┘         └──────────────┘         └─────────────┘
```

## Setup Process

### Step 1: Power On the Device

When the Smart Agent boots:
1. It initializes the display and shows "WiFi Setup"
2. Creates an Access Point named `Buddy-XXXX` (e.g., `Buddy-A3F2`)
3. Starts a web server on `192.168.4.1`
4. Displays instructions on the AMOLED screen

### Step 2: Connect Your Phone

1. Open your phone's WiFi settings
2. Look for a network named `Buddy-XXXX` (random 4-character suffix)
3. Connect to this network (no password required)

### Step 3: Open Web Interface

1. Open any web browser on your phone
2. Navigate to `http://192.168.4.1`
3. The WiFi setup page will load automatically

### Step 4: Enter WiFi Credentials

1. Enter your home WiFi network name (SSID)
2. Enter your WiFi password
3. Tap "Connect"
4. Wait for confirmation

### Step 5: Device Connects

The device will:
1. Save your credentials
2. Connect to your WiFi network
3. Display its new IP address
4. Show "AI Ready" status
5. Stop the web server (for security)

## Web Interface

The provisioning interface provides:

- **Network Name**: Shows current AP name (Buddy-XXXX)
- **WiFi SSID Input**: Enter your network name
- **Password Input**: Enter your WiFi password
- **Connect Button**: Submit credentials
- **Status Updates**: Real-time connection feedback

### Interface Design

```
┌─────────────────────┐
│    Buddy WiFi Setup │
├─────────────────────┤
│  Network: Buddy-A3F2│
│  Access: 192.168.4.1│
├─────────────────────┤
│  WiFi Network       │
│  [____________]     │
│                     │
│  WiFi Password      │
│  [____________]     │
│                     │
│  [   Connect   ]    │
└─────────────────────┘
```

## Technical Details

### AP Configuration

| Parameter | Value |
|-----------|-------|
| SSID Prefix | `Buddy-` |
| SSID Suffix | Random 4 hex characters |
| IP Address | `192.168.4.1` |
| Netmask | `255.255.255.0` |
| Channel | 1 |
| Authentication | Open (no password) |
| Max Connections | 4 |

### API Endpoints

The web server provides these REST endpoints:

#### GET /wifi
Returns current WiFi status as JSON.

**Response:**
```json
{
  "ssid": "MyHomeWiFi",
  "ip": "192.168.1.100",
  "status": "Connected"
}
```

Status values: `AP`, `Connected`, `Connecting`, `Disconnected`, `Error`

#### GET /ollama
Returns Ollama server status.

**Response:**
```json
{
  "available": false,
  "address": "http://localhost:11434",
  "model": "llama2"
}
```

#### POST /wifi_submit
Submit WiFi credentials.

**Request:**
```json
{
  "ssid": "MyHomeWiFi",
  "password": "mysecretpassword"
}
```

**Response:**
```
WiFi credentials submitted successfully
```

#### GET /
Returns the HTML provisioning page.

## Security Considerations

### During Provisioning

- The AP is open (no password) for easy access
- Only one device can configure at a time
- The web server stops after successful connection
- Credentials are transmitted in plaintext (HTTP, not HTTPS)

### Best Practices

1. **Complete setup quickly** - Don't leave device in AP mode
2. **Use at home** - Don't configure in public spaces
3. **Secure your WiFi** - Use WPA2 on your home network
4. **Check the display** - Verify connection status

## Troubleshooting

### Can't See Buddy-XXXX Network

1. **Wait 10 seconds** - AP takes time to start
2. **Check display** - Should show "WiFi Setup" instructions
3. **Restart device** - Power cycle and try again
4. **Check distance** - Stay within 10 feet of device

### Web Page Won't Load

1. **Verify connection** - Ensure phone shows connected to Buddy-XXXX
2. **Use IP address** - Type `192.168.4.1` (not buddy.local)
3. **Try different browser** - Some browsers cache redirects
4. **Disable mobile data** - Some phones need this for captive portal

### Credentials Not Working

1. **Check SSID** - Ensure exact match (case-sensitive)
2. **Check password** - Re-enter carefully
3. **Verify WiFi type** - Supports WPA/WPA2, not WEP
4. **Check 2.4GHz** - Device only supports 2.4GHz networks
5. **Signal strength** - Device may be too far from router

### Device Won't Connect

1. **Check router** - Ensure 2.4GHz band is enabled
2. **MAC filtering** - Add device MAC to allow list if enabled
3. **Special characters** - Some characters in passwords may cause issues
4. **Restart router** - Try power cycling your WiFi router

## Resetting WiFi Credentials

To reconfigure WiFi:

1. **Hold reset button** (if available) for 5 seconds
2. **Or use voice command** - Say "Reset" to reboot
3. **Device will restart** in AP mode
4. **Repeat setup process**

## Advanced Configuration

### Manual Credential Storage

Credentials are stored in ESP32 NVS (Non-Volatile Storage):

```cpp
// Namespace: "wifi_config"
// Keys:
//   - "ssid"     : WiFi network name
//   - "password" : WiFi password
//   - "configured": "1" if credentials saved
```

### Programmatic Access

```cpp
#include "wifi_manager.h"

WiFiManager wifi;
wifi.init();

// Check if credentials exist
if (wifi.hasStoredCredentials()) {
    wifi.connectWithStoredCredentials();
} else {
    wifi.startAPMode("Buddy-1234");
}
```

## Display Messages

During provisioning, the AMOLED display shows:

| Stage | Message |
|-------|---------|
| Boot | "Initializing..." |
| AP Mode | "WiFi Setup" |
| | "Please connect" |
| | "to 'Buddy-XXXX'" |
| | "Then open:" |
| | "192.168.4.1" |
| Connected | "WiFi Connected" |
| | [SSID name] |
| | [IP address] |
| | "AI Ready" |

## LED Indicators (if available)

| Pattern | Meaning |
|---------|---------|
| Solid Blue | AP mode active |
| Blinking Blue | Connecting to WiFi |
| Solid Green | Connected to WiFi |
| Blinking Red | Connection failed |

## Future Enhancements

Planned improvements to the provisioning system:

- [ ] **NVS Persistence** - Store credentials across reboots
- [ ] **QR Code Provisioning** - Scan QR to auto-configure
- [ ] **Multiple Networks** - Store fallback networks
- [ ] **WPS Support** - Push-button WiFi setup
- [ ] **BLE Provisioning** - Configure via Bluetooth
- [ ] **HTTPS Support** - Secure credential transmission

## Related Documentation

- [README.md](README.md) - Main project documentation
- [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md) - Project overview
- ESP-IDF WiFi Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review ESP-IDF WiFi examples
3. Check project GitHub issues
