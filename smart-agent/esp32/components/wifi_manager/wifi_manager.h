#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <string>
#include <memory>
#include "esp_event.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"

/**
 * @brief WiFi connection status
 */
enum class WifiStatus {
    AP_MODE,
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

/**
 * @brief WiFi manager for AP and station mode
 */
class WiFiManager {
public:
    WiFiManager();
    ~WiFiManager();

    esp_err_t init();
    esp_err_t startAPMode(const char* ssid, const char* password = nullptr);
    esp_err_t connectToWiFi(const char* ssid, const char* password);
    esp_err_t disconnect();
    WifiStatus getStatus() const;
    std::string getIP() const;
    std::string getSSID() const;
    bool isConnected() const;
    std::string getAPIP() const;
    void handleEvent(esp_event_base_t event_base, int32_t event_id, void* event_data);

    // NVS persistence methods
    esp_err_t saveCredentials(const char* ssid, const char* password);
    esp_err_t loadCredentials(std::string& ssid, std::string& password);
    bool hasStoredCredentials() const;
    esp_err_t clearStoredCredentials();
    esp_err_t connectWithStoredCredentials();

private:
    esp_netif_t* m_ap_netif;
    esp_netif_t* m_sta_netif;
    WifiStatus m_status;
    std::string m_ssid;
    std::string m_ip;
    std::string m_ap_ip;
    bool m_ap_started;
};

#endif // WIFI_MANAGER_H