#include "web_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "WebServer";

WebServer::WebServer()
    : m_server(nullptr)
    , m_wifi_manager(nullptr)
{
}

WebServer::~WebServer()
{
    stop();
}

esp_err_t WebServer::init()
{
    ESP_LOGI(TAG, "Web server initialization");
    return ESP_OK;
}

esp_err_t WebServer::start(const char* ap_ip)
{
    if (m_server != nullptr) {
        ESP_LOGW(TAG, "Server already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting web server on %s", ap_ip);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;

    httpd_uri_t wifi_status = {
        .uri = "/wifi",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            WebServer* server = (WebServer*)req->user_ctx;
            return server->handleWiFiStatus(req);
        },
        .user_ctx = this
    };

    httpd_uri_t ollama_status = {
        .uri = "/ollama",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            WebServer* server = (WebServer*)req->user_ctx;
            return server->handleOllamaStatus(req);
        },
        .user_ctx = this
    };

    httpd_uri_t wifi_submit = {
        .uri = "/wifi_submit",
        .method = HTTP_POST,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            WebServer* server = (WebServer*)req->user_ctx;
            return server->handleWiFiSubmit(req);
        },
        .user_ctx = this
    };

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = [](httpd_req_t* req) -> esp_err_t {
            WebServer* server = (WebServer*)req->user_ctx;
            return server->handleRoot(req);
        },
        .user_ctx = this
    };

    if (httpd_start(&m_server, &config) == ESP_OK) {
        httpd_register_uri_handler(m_server, &root);
        httpd_register_uri_handler(m_server, &wifi_status);
        httpd_register_uri_handler(m_server, &ollama_status);
        httpd_register_uri_handler(m_server, &wifi_submit);

        ESP_LOGI(TAG, "Web server started successfully");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

esp_err_t WebServer::stop()
{
    if (m_server != nullptr) {
        ESP_LOGI(TAG, "Stopping web server");
        httpd_stop(m_server);
        m_server = nullptr;
    }
    return ESP_OK;
}

esp_err_t WebServer::handleRequest(httpd_req_t* req)
{
    const char* uri = req->uri;
    ESP_LOGI(TAG, "Request: %s", uri);

    if (strcmp(uri, "/wifi") == 0) {
        return handleWiFiStatus(req);
    } else if (strcmp(uri, "/ollama") == 0) {
        return handleOllamaStatus(req);
    } else if (strcmp(uri, "/wifi_submit") == 0) {
        return handleWiFiSubmit(req);
    } else if (strcmp(uri, "/") == 0) {
        return handleRoot(req);
    }

    return httpd_resp_send(req, "Not Found", -1);
}

esp_err_t WebServer::handleWiFiCredentials(const char* ssid, const char* password)
{
    ESP_LOGI(TAG, "WiFi credentials received: %s", ssid);

    if (m_wifi_manager == nullptr) {
        ESP_LOGE(TAG, "WiFi manager not set");
        return ESP_FAIL;
    }

    // Save credentials to NVS first
    esp_err_t ret = m_wifi_manager->saveCredentials(ssid, password);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save credentials to NVS: %s", esp_err_to_name(ret));
        // Continue anyway - try to connect even if save failed
    }

    // Connect to WiFi
    ret = m_wifi_manager->connectToWiFi(ssid, password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi connection initiated successfully");
    return ESP_OK;
}

std::string WebServer::getWiFiStatus() const
{
    cJSON* root = cJSON_CreateObject();

    if (m_wifi_manager != nullptr) {
        cJSON_AddStringToObject(root, "ssid", m_wifi_manager->getSSID().c_str());
        cJSON_AddStringToObject(root, "ip", m_wifi_manager->getIP().c_str());

        std::string statusStr;
        switch (m_wifi_manager->getStatus()) {
            case WifiStatus::AP_MODE:
                statusStr = "AP";
                break;
            case WifiStatus::CONNECTED:
                statusStr = "Connected";
                break;
            case WifiStatus::CONNECTING:
                statusStr = "Connecting";
                break;
            case WifiStatus::DISCONNECTED:
                statusStr = "Disconnected";
                break;
            case WifiStatus::ERROR:
                statusStr = "Error";
                break;
        }
        cJSON_AddStringToObject(root, "status", statusStr.c_str());
    } else {
        cJSON_AddStringToObject(root, "status", "Unknown");
    }

    char* json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    free(json_str);
    cJSON_Delete(root);

    return result;
}

std::string WebServer::getOllamaStatus() const
{
    cJSON* root = cJSON_CreateObject();

    // Check if localhost:11434 is accessible
    bool ollama_available = false;
    cJSON_AddBoolToObject(root, "available", ollama_available);
    cJSON_AddStringToObject(root, "address", "http://localhost:11434");
    cJSON_AddStringToObject(root, "model", "llama2");

    char* json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    free(json_str);
    cJSON_Delete(root);

    return result;
}

esp_err_t WebServer::handleRoot(httpd_req_t* req)
{
    sendHTML(req);
    return ESP_OK;
}

esp_err_t WebServer::handleWiFiSubmit(httpd_req_t* req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send(req, "Timeout", -1);
        } else {
            httpd_resp_send(req, "Failed", -1);
        }
        return ESP_FAIL;
    }

    buf[ret] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (root == nullptr) {
        httpd_resp_send(req, "Invalid JSON", -1);
        return ESP_FAIL;
    }

    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* password = cJSON_GetObjectItem(root, "password");

    if (ssid == nullptr || ssid->valuestring == nullptr) {
        httpd_resp_send(req, "Missing SSID", -1);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    httpd_resp_send(req, "WiFi credentials submitted successfully", -1);

    if (m_wifi_manager != nullptr && password != nullptr && password->valuestring != nullptr) {
        handleWiFiCredentials(ssid->valuestring, password->valuestring);
    } else if (m_wifi_manager != nullptr) {
        handleWiFiCredentials(ssid->valuestring, nullptr);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t WebServer::handleWiFiStatus(httpd_req_t* req)
{
    std::string json = getWiFiStatus();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());

    return ESP_OK;
}

esp_err_t WebServer::handleOllamaStatus(httpd_req_t* req)
{
    std::string json = getOllamaStatus();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());

    return ESP_OK;
}

void WebServer::sendHTML(httpd_req_t* req)
{
    const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Buddy WiFi Setup</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 400px;
            width: 100%;
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 20px;
            font-size: 24px;
        }
        .info {
            background: #f0f0f0;
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 20px;
            text-align: center;
        }
        .info p {
            color: #666;
            margin: 5px 0;
            font-size: 14px;
        }
        .info strong {
            color: #333;
        }
        .form-group {
            margin-bottom: 20px;
        }
        .form-group label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: bold;
        }
        .form-group input {
            width: 100%;
            padding: 12px;
            border: 2px solid #ddd;
            border-radius: 10px;
            font-size: 16px;
            transition: border-color 0.3s;
        }
        .form-group input:focus {
            outline: none;
            border-color: #667eea;
        }
        button {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 20px rgba(0,0,0,0.2);
        }
        button:disabled {
            background: #ccc;
            cursor: not-allowed;
            transform: none;
            box-shadow: none;
        }
        .status {
            margin-top: 20px;
            text-align: center;
            font-size: 14px;
            padding: 10px;
            border-radius: 10px;
            display: none;
        }
        .success {
            background: #d4edda;
            color: #155724;
        }
        .error {
            background: #f8d7da;
            color: #721c24;
        }
        .loading {
            display: none;
            text-align: center;
            padding: 20px;
        }
        .spinner {
            border: 4px solid #f3f3f3;
            border-top: 4px solid #667eea;
            border-radius: 50%;
            width: 40px;
            height: 40px;
            animation: spin 1s linear infinite;
            margin: 0 auto 15px;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Buddy WiFi Setup</h1>

        <div class="info">
            <p><strong>Network:</strong> Buddy-XXXX</p>
            <p><strong>Access:</strong> http://192.168.4.1</p>
        </div>

        <form id="wifiForm">
            <div class="form-group">
                <label for="ssid">WiFi Network</label>
                <input type="text" id="ssid" name="ssid" required placeholder="Enter WiFi SSID">
            </div>

            <div class="form-group">
                <label for="password">WiFi Password</label>
                <input type="password" id="password" name="password" placeholder="Enter WiFi password">
            </div>

            <button type="submit" id="submitBtn">Connect</button>
        </form>

        <div id="status" class="status"></div>
        <div id="loading" class="loading">
            <div class="spinner"></div>
            <p>Connecting...</p>
        </div>
    </div>

    <script>
        document.getElementById('wifiForm').addEventListener('submit', async function(e) {
            e.preventDefault();

            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;
            const status = document.getElementById('status');
            const loading = document.getElementById('loading');
            const submitBtn = document.getElementById('submitBtn');

            status.style.display = 'none';
            loading.style.display = 'block';
            submitBtn.disabled = true;

            try {
                const response = await fetch('/wifi_submit', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({ ssid: ssid, password: password })
                });

                const result = await response.text();

                if (response.ok) {
                    status.textContent = 'WiFi credentials submitted successfully!';
                    status.className = 'status success';
                } else {
                    status.textContent = 'Failed to submit credentials';
                    status.className = 'status error';
                }
            } catch (error) {
                status.textContent = 'Connection error: ' + error.message;
                status.className = 'status error';
            } finally {
                loading.style.display = 'none';
                submitBtn.disabled = false;
            }
        });

        // Check WiFi status periodically
        setInterval(async function() {
            try {
                const response = await fetch('/wifi');
                const data = await response.json();

                if (data.status === 'Connected') {
                    document.querySelector('.container').innerHTML =
                        '<div style="text-align:center; padding:40px;">' +
                        '<h2 style="color:#155724; margin-bottom:20px;">✓ Connected!</h2>' +
                        '<p style="color:#666; margin-bottom:20px;">Your Buddy device is now connected to WiFi.</p>' +
                        '<p style="color:#666; margin-bottom:10px;">IP: ' + data.ip + '</p>' +
                        '<p style="color:#666; margin-bottom:20px;">You can now talk to your AI assistant!</p>' +
                        '</div>';
                }
            } catch (error) {
                console.error('WiFi status check failed:', error);
            }
        }, 3000);
    </script>
</body>
</html>
    )rawliteral";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));
}