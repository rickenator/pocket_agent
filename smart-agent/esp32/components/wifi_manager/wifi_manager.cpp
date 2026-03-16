#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/semphr.h"
#include <cstring>
#include <arpa/inet.h>

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager()
    : m_ap_netif(nullptr)
    , m_sta_netif(nullptr)
    , m_status(WifiStatus::DISCONNECTED)
    , m_ap_started(false)
{
}

WiFiManager::~WiFiManager()
{
    disconnect();
}

esp_err_t WiFiManager::init()
{
    ESP_LOGI(TAG, "WiFi manager initialization");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP stack
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create AP netif
    m_ap_netif = esp_netif_create_default_wifi_ap();
    if (m_ap_netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create AP netif");
        return ESP_FAIL;
    }

    // Create STA netif
    m_sta_netif = esp_netif_create_default_wifi_sta();
    if (m_sta_netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create STA netif");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    return ESP_OK;
}

esp_err_t WiFiManager::startAPMode(const char* ssid, const char* password)
{
    if (m_ap_started) {
        ESP_LOGW(TAG, "AP already started");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting AP mode with SSID: %s", ssid);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(ssid);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;

    if (password != nullptr && strlen(password) > 0) {
        strncpy((char*)wifi_config.ap.password, password, sizeof(wifi_config.ap.password) - 1);
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_config.ap.ssid_hidden = 0;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        wifi_config.ap.ssid_hidden = 0;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    esp_netif_ip_info_t ip_info = {};
    uint32_t ap_ip = (192 << 24) | (168 << 16) | (4 << 8) | 1;
    uint32_t ap_netmask = (255 << 24) | (255 << 16) | (255 << 8) | 0;
    ip_info.ip.addr = htonl(ap_ip);
    ip_info.netmask.addr = htonl(ap_netmask);
    ip_info.gw.addr = htonl(ap_ip);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(m_ap_netif, &ip_info));

    ESP_ERROR_CHECK(esp_wifi_start());

    m_status = WifiStatus::AP_MODE;
    m_ssid = ssid;
    m_ap_ip = "192.168.4.1";
    m_ap_started = true;

    ESP_LOGI(TAG, "AP started successfully. IP: %s", m_ap_ip.c_str());
    return ESP_OK;
}

esp_err_t WiFiManager::connectToWiFi(const char* ssid, const char* password)
{
    if (m_status == WifiStatus::CONNECTED) {
        ESP_LOGW(TAG, "Already connected to WiFi");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    m_status = WifiStatus::CONNECTING;
    m_ssid = ssid;

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);

    if (password != nullptr && strlen(password) > 0) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

esp_err_t WiFiManager::disconnect()
{
    if (m_status == WifiStatus::AP_MODE) {
        ESP_ERROR_CHECK(esp_wifi_stop());
        m_ap_started = false;
        m_status = WifiStatus::DISCONNECTED;
        ESP_LOGI(TAG, "AP stopped");
    } else if (m_status == WifiStatus::CONNECTED || m_status == WifiStatus::CONNECTING) {
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        m_status = WifiStatus::DISCONNECTED;
        m_ssid = "";
        m_ip = "";
        ESP_LOGI(TAG, "WiFi disconnected");
    }

    return ESP_OK;
}

WifiStatus WiFiManager::getStatus() const
{
    return m_status;
}

std::string WiFiManager::getIP() const
{
    return m_ip;
}

std::string WiFiManager::getSSID() const
{
    return m_ssid;
}

bool WiFiManager::isConnected() const
{
    return m_status == WifiStatus::CONNECTED;
}

void WiFiManager::handleEvent(esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* sta_event =
                    (wifi_event_ap_staconnected_t*)event_data;
                ESP_LOGI(TAG, "Station connected: %d", sta_event->mac[5]);
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* sta_event =
                    (wifi_event_ap_stadisconnected_t*)event_data;
                ESP_LOGI(TAG, "Station disconnected: %d", sta_event->mac[5]);
                break;
            }

            case WIFI_EVENT_STA_START: {
                ESP_LOGI(TAG, "WiFi station started");
                esp_wifi_connect();
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED: {
                ESP_LOGI(TAG, "WiFi disconnected");
                m_status = WifiStatus::DISCONNECTED;
                m_ssid = "";
                m_ip = "";

                // Reconnect after delay
                if (m_status != WifiStatus::AP_MODE) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_wifi_connect();
                }
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ESP_LOGI(TAG, "WiFi got IP");
            m_status = WifiStatus::CONNECTED;

            // Get IP address
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(m_sta_netif, &ip_info) == ESP_OK) {
                char ip_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
                m_ip = ip_str;
                ESP_LOGI(TAG, "IP: %s", m_ip.c_str());
            }
        }
    }
}

std::string WiFiManager::getAPIP() const
{
    return m_ap_ip;
}

// NVS Persistence Implementation

static const char* NVS_NAMESPACE = "wifi_config";
static const char* NVS_KEY_SSID = "ssid";
static const char* NVS_KEY_PASSWORD = "password";
static const char* NVS_KEY_CONFIGURED = "configured";

esp_err_t WiFiManager::saveCredentials(const char* ssid, const char* password)
{
    if (ssid == nullptr || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "Cannot save empty SSID");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    // Save SSID
    err = nvs_set_str(nvs_handle, NVS_KEY_SSID, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Save password (can be empty for open networks)
    if (password != nullptr) {
        err = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, password);
    } else {
        err = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, "");
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Mark as configured
    err = nvs_set_u8(nvs_handle, NVS_KEY_CONFIGURED, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configured flag: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t WiFiManager::loadCredentials(std::string& ssid, std::string& password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No WiFi config found in NVS");
        return err;
    }

    // Read SSID
    size_t ssid_len = 0;
    err = nvs_get_str(nvs_handle, NVS_KEY_SSID, nullptr, &ssid_len);
    if (err == ESP_OK && ssid_len > 0) {
        char* ssid_buf = new char[ssid_len];
        err = nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid_buf, &ssid_len);
        if (err == ESP_OK) {
            ssid = ssid_buf;
        }
        delete[] ssid_buf;
    }

    // Read password
    size_t pass_len = 0;
    err = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, nullptr, &pass_len);
    if (err == ESP_OK && pass_len > 0) {
        char* pass_buf = new char[pass_len];
        err = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, pass_buf, &pass_len);
        if (err == ESP_OK) {
            password = pass_buf;
        }
        delete[] pass_buf;
    }

    nvs_close(nvs_handle);

    if (ssid.empty()) {
        ESP_LOGW(TAG, "Loaded credentials but SSID is empty");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "WiFi credentials loaded from NVS (SSID: %s)", ssid.c_str());
    return ESP_OK;
}

bool WiFiManager::hasStoredCredentials() const
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }

    uint8_t configured = 0;
    err = nvs_get_u8(nvs_handle, NVS_KEY_CONFIGURED, &configured);
    nvs_close(nvs_handle);

    return (err == ESP_OK && configured == 1);
}

esp_err_t WiFiManager::clearStoredCredentials()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No NVS namespace to clear");
        return err;
    }

    // Erase all keys
    nvs_erase_key(nvs_handle, NVS_KEY_SSID);
    nvs_erase_key(nvs_handle, NVS_KEY_PASSWORD);
    nvs_erase_key(nvs_handle, NVS_KEY_CONFIGURED);

    err = nvs_commit(nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi credentials cleared from NVS");
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t WiFiManager::connectWithStoredCredentials()
{
    std::string ssid, password;
    esp_err_t err = loadCredentials(ssid, password);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No stored credentials to connect with");
        return err;
    }

    ESP_LOGI(TAG, "Connecting with stored credentials to: %s", ssid.c_str());
    return connectToWiFi(ssid.c_str(), password.empty() ? nullptr : password.c_str());
}