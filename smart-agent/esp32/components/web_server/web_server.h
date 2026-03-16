#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include "esp_event.h"
#include "esp_http_server.h"
#include "wifi_manager.h"

/**
 * @brief Web server for WiFi provisioning
 */
class WebServer {
public:
    WebServer();
    ~WebServer();

    /**
     * @brief Initialize web server
     * @return ESP_OK on success
     */
    esp_err_t init();

    /**
     * @brief Start web server on AP IP
     * @param ap_ip AP IP address string
     * @return ESP_OK on success
     */
    esp_err_t start(const char* ap_ip);

    /**
     * @brief Stop web server
     * @return ESP_OK on success
     */
    esp_err_t stop();

    /**
     * @brief Handle HTTP requests
     * @param req HTTP request
     * @return esp_err_t
     */
    esp_err_t handleRequest(httpd_req_t* req);

    /**
     * @brief Handle WiFi credentials submission
     * @param ssid WiFi SSID
     * @param password WiFi password
     * @return ESP_OK on success
     */
    esp_err_t handleWiFiCredentials(const char* ssid, const char* password);

    /**
     * @brief Get WiFi status
     * @return JSON string with WiFi status
     */
    std::string getWiFiStatus() const;

    /**
     * @brief Get Ollama status
     * @return JSON string with Ollama status
     */
    std::string getOllamaStatus() const;

private:
    httpd_handle_t m_server;
    WiFiManager* m_wifi_manager;

    esp_err_t handleRoot(httpd_req_t* req);
    esp_err_t handleWiFiSubmit(httpd_req_t* req);
    esp_err_t handleWiFiStatus(httpd_req_t* req);
    esp_err_t handleOllamaStatus(httpd_req_t* req);
    void sendHTML(httpd_req_t* req);
};

#endif // WEB_SERVER_H