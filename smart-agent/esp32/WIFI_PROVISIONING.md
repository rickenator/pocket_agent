# WiFi Provisioning System

## Overview

The smart agent device uses an IoT-style WiFi provisioning system that allows easy network configuration through a web interface. When powered on, the device creates a temporary WiFi access point for provisioning.

## Features

- **AP Mode**: Creates a local WiFi access point for device discovery
- **Web-based Configuration**: Simple browser-based WiFi credential entry
- **Auto-Reconnect**: Automatic reconnection when WiFi signal is lost
- **Status Display**: Real-time WiFi connection status on the device display

## System Architecture

### Components

1. **WiFi Manager** (`components/wifi_manager/`)
   - Manages AP and station mode operations
   - Handles WiFi events and connection status
   - Provides IP address and SSID information

2. **Web Server** (`components/web_server/`)
   - Serves provisioning interface at http://192.168.4.1
   - Handles WiFi credential submission
   - Provides WiFi and Ollama status endpoints

3. **Main Application** (`main/main.cpp`)
   - Integrates WiFi provisioning with AI agent system
   - Monitors WiFi connection status
   - Displays connection information on OLED screen

## Setup Process

### 1. Device Startup

When the device powers on:

1. WiFi manager creates an AP with SSID format: `Buddy-XXXX`
2. Display shows WiFi setup information
3. Web server starts on IP `192.168.4.1`

Example AP SSID: `Buddy-A3F2`

### 2. Provisioning

1. On a smartphone or computer, connect to the device's AP
2. Open a web browser and navigate to: `http://192.168.4.1`
3. Enter your WiFi network SSID and password
4. Click "Connect"leit
5. Device attempts to connect to your WiFi network

### 3. Connection Confirmation

After successful WiFi connection:

1. Display shows "WiFi Connected" with SSID and IP
2. Web server stops automatically
3. Device is ready for AI agent operations

## Technical Details

### AP Configuration

- **Network**: Buddy-XXXX
- **IP Address**: 192.168.4.1
- **Channel**: 1
- **Authentication**: WPA2-PSK (optional: open)
- **Max Connections**: 4

### WiFi Manager

The WiFi manager handles:

- AP mode initialization
- Station mode connection
- Event handling (connect/disconnect)
- IP address management
- Status monitoring

### Web Server Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Provisioning interface |
| `/wifi` | GET | WiFi status JSON |
| `/ollama` | GET | Ollama status JSON |
| `/wifi_submit` | POST | WiFi credential submission |

### WiFi Status JSON Response

```json
{
  "ssid": "YourWiFiNetwork",
  "ip": "192.168.1.100",
  "status": "Connected"
}
```

## Troubleshooting

### Device Not Found

- Ensure you're on the device's WiFi network
- Check device IP: `192.168.4.1`
- Try restarting the device

### Connection Failed

- Verify WiFi credentials are correct
- Ensure WiFi is within range
- Check for signal interference

### Display Issues

- Device must be on to provision WiFi
- Display shows WiFi setup when provisioning
- Display shows connection info when connected

## Security Considerations

- AP mode is temporary (stops when connected to WiFi)
- Use WPA2-PSK for secure home networks
- AP password is optional (useful for public devices)

## Future Enhancements

- [ ] WiFi credential persistence (NVS)
- [ ] QR code provisioning
- [ ] MQTT broker integration
- [ ] Remote OTA updates
- [ ] Multiple network profiles

## API Documentation

See individual component headers for detailed API documentation.

### WiFiManager

```cpp
class WiFiManager {
public:
    esp_err_t init();
    esp_err_t startAPMode(const char* ssid, const char* password);
    esp_err_t connectToWiFi(const char* ssid, const char* password);
    esp_err_t disconnect();
    WifiStatus getStatus() const;
    std::string getIP() const;
    std::string getSSID() const;
    bool isConnected() const;
};
```

### WebServer

```cpp
class WebServer {
public:
    esp_err_t init();
    esp_err_t start(const char* ap_ip);
    esp_err_t stop();
    esp_err_t handleWiFiCredentials(const char* ssid, const char* password);
    std::string getWiFiStatus() const;
    std::string getOllamaStatus() const;
};
```