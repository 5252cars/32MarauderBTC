/**
 * @file WiFiConfigManager.cpp
 * @brief Implementation of WiFi Configuration Manager for K5MarauderBTC
 * @author K5MarauderBTC
 * @date 2025-03-29
 */

#include "WiFiConfigManager.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "dns_server.h"

// Static tag for logging
static const char* TAG = "WIFI_CONFIG";

// HTML for captive portal
static const char* HTML_HEADER = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>K5MarauderBTC Setup</title><style>body{background:#121212;color:#f8f9fa;font-family:Arial,sans-serif;margin:0;padding:20px}h1{color:#f7931a}input,select{width:100%;padding:8px;margin:8px 0;box-sizing:border-box;background:#333;color:#fff;border:1px solid #555}button{background:#f7931a;color:#fff;padding:10px 15px;border:none;cursor:pointer;width:100%}button:hover{background:#e68a19}.card{background:#1e1e1e;border-radius:5px;padding:20px;margin-bottom:20px}</style></head><body><h1>K5MarauderBTC Setup</h1>";
static const char* HTML_WIFI_FORM = "<div class=\"card\"><h2>WiFi Configuration</h2><form id=\"wifiForm\"><div><label for=\"ssid\">WiFi SSID:</label><select id=\"ssid\" name=\"ssid\">{{WIFI_LIST}}</select></div><div><label for=\"password\">WiFi Password:</label><input type=\"password\" id=\"password\" name=\"password\"></div><button type=\"button\" onclick=\"scanWiFi()\">Scan for Networks</button></div>";
static const char* HTML_SETTINGS_FORM = "<div class=\"card\"><h2>BTC Clock Settings</h2><div><label for=\"currency\">Currency:</label><select id=\"currency\" name=\"currency\"><option value=\"usd\">USD ($)</option><option value=\"eur\">EUR (€)</option><option value=\"gbp\">GBP (£)</option><option value=\"jpy\">JPY (¥)</option></select></div><div><label for=\"interval\">Update Interval (seconds):</label><input type=\"number\" id=\"interval\" name=\"interval\" min=\"30\" max=\"3600\" value=\"60\"></div><div><label for=\"autostart\">Auto-start BTC Clock:</label><input type=\"checkbox\" id=\"autostart\" name=\"autostart\" checked></div><button type=\"submit\">Save Configuration</button></form></div>";
static const char* HTML_FOOTER = "<script>function scanWiFi(){fetch('/scan').then(r=>r.json()).then(data=>{let s=document.getElementById('ssid');s.innerHTML='';data.forEach(n=>{let o=document.createElement('option');o.value=n;o.text=n;s.appendChild(o);});});}document.getElementById('wifiForm').addEventListener('submit',function(e){e.preventDefault();let f=new FormData(this);fetch('/save',{method:'POST',body:f}).then(r=>r.text()).then(t=>{alert(t);});});</script></body></html>";

// DNS server for captive portal
static DNSServer dns_server;

// Constructor
WiFiConfigManager::WiFiConfigManager(const std::string& ap_ssid,
                                     const std::string& ap_password,
                                     const std::string& nvs_namespace)
    : server(nullptr),
      ap_ssid(ap_ssid),
      ap_password(ap_password),
      nvs_namespace(nvs_namespace),
      wifi_configured(false),
      captive_portal_active(false)
{
    // Set default BTC Clock settings
    btc_settings.currency = "usd";
    btc_settings.update_interval = 60;
    btc_settings.auto_start = true;
}

// Destructor
WiFiConfigManager::~WiFiConfigManager() {
    stopConfigPortal();
}

// Initialize the WiFi Configuration Manager
bool WiFiConfigManager::init() {
    ESP_LOGI(TAG, "Initializing WiFi Configuration Manager");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Load settings from NVS
    if (loadSettings()) {
        ESP_LOGI(TAG, "Settings loaded from NVS");
        wifi_configured = !wifi_credentials.ssid.empty();
    } else {
        ESP_LOGI(TAG, "No settings found in NVS");
        wifi_configured = false;
    }
    
    return true;
}

// Check if WiFi is configured
bool WiFiConfigManager::isWiFiConfigured() const {
    return wifi_configured;
}

// Start the captive portal for WiFi configuration
bool WiFiConfigManager::startConfigPortal() {
    if (captive_portal_active) {
        ESP_LOGI(TAG, "Captive portal already active");
        return true;
    }
    
    ESP_LOGI(TAG, "Starting captive portal");
    
    // Start AP mode
    if (!startAP()) {
        ESP_LOGE(TAG, "Failed to start AP mode");
        return false;
    }
    
    // Start captive portal
    if (!startCaptivePortal()) {
        ESP_LOGE(TAG, "Failed to start captive portal");
        return false;
    }
    
    captive_portal_active = true;
    return true;
}

// Stop the captive portal
void WiFiConfigManager::stopConfigPortal() {
    if (!captive_portal_active) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping captive portal");
    
    // Stop DNS server
    dns_server.stop();
    
    // Stop HTTP server
    if (server) {
        httpd_stop(server);
        server = nullptr;
    }
    
    // Stop AP mode
    esp_wifi_stop();
    
    captive_portal_active = false;
}

// Connect to WiFi using stored credentials
bool WiFiConfigManager::connectToWiFi() {
    if (!wifi_configured) {
        ESP_LOGE(TAG, "WiFi not configured");
        return false;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", wifi_credentials.ssid.c_str());
    
    // Initialize WiFi in station mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // Configure WiFi station
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, wifi_credentials.ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, wifi_credentials.password.c_str(), sizeof(wifi_config.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    // Wait for connection
    int retry_count = 0;
    while (retry_count < 10) {
        wifi_ap_record_t ap_info;
        esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Connected to WiFi SSID: %s", wifi_credentials.ssid.c_str());
            return true;
        }
        
        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
    }
    
    ESP_LOGE(TAG, "Failed to connect to WiFi");
    return false;
}

// Get the WiFi credentials
const WiFiCredentials& WiFiConfigManager::getWiFiCredentials() const {
    return wifi_credentials;
}

// Get the BTC Clock settings
const BTCClockSettings& WiFiConfigManager::getBTCClockSettings() const {
    return btc_settings;
}

// Set the WiFi credentials
bool WiFiConfigManager::setWiFiCredentials(const std::string& ssid, const std::string& password) {
    wifi_credentials.ssid = ssid;
    wifi_credentials.password = password;
    wifi_configured = !ssid.empty();
    
    return saveSettings();
}

// Set the BTC Clock settings
bool WiFiConfigManager::setBTCClockSettings(const std::string& currency, int update_interval, bool auto_start) {
    btc_settings.currency = currency;
    btc_settings.update_interval = update_interval;
    btc_settings.auto_start = auto_start;
    
    return saveSettings();
}

// Reset all settings to defaults
bool WiFiConfigManager::resetSettings() {
    wifi_credentials.ssid = "";
    wifi_credentials.password = "";
    
    btc_settings.currency = "usd";
    btc_settings.update_interval = 60;
    btc_settings.auto_start = true;
    
    wifi_configured = false;
    
    // Clear NVS
    nvs_handle_t handle;
    esp_err_t err = nvs_open(nvs_namespace.c_str(), NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_erase_all(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Settings reset to defaults");
    return true;
}

// Process captive portal requests
void WiFiConfigManager::process() {
    // Nothing to do here, HTTP server is handled by ESP-IDF
}

// Load settings from NVS
bool WiFiConfigManager::loadSettings() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(nvs_namespace.c_str(), NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    // Load WiFi credentials
    size_t ssid_len = 0;
    size_t password_len = 0;
    
    // Get lengths first
    err = nvs_get_str(handle, "wifi_ssid", nullptr, &ssid_len);
    if (err == ESP_OK && ssid_len > 0) {
        char* ssid_buf = new char[ssid_len];
        err = nvs_get_str(handle, "wifi_ssid", ssid_buf, &ssid_len);
        if (err == ESP_OK) {
            wifi_credentials.ssid = ssid_buf;
        }
        delete[] ssid_buf;
    }
    
    err = nvs_get_str(handle, "wifi_pass", nullptr, &password_len);
    if (err == ESP_OK && password_len > 0) {
        char* password_buf = new char[password_len];
        err = nvs_get_str(handle, "wifi_pass", password_buf, &password_len);
        if (err == ESP_OK) {
            wifi_credentials.password = password_buf;
        }
        delete[] password_buf;
    }
    
    // Load BTC Clock settings
    size_t currency_len = 0;
    err = nvs_get_str(handle, "currency", nullptr, &currency_len);
    if (err == ESP_OK && currency_len > 0) {
        char* currency_buf = new char[currency_len];
        err = nvs_get_str(handle, "currency", currency_buf, &currency_len);
        if (err == ESP_OK) {
            btc_settings.currency = currency_buf;
        }
        delete[] currency_buf;
    }
    
    int32_t update_interval;
    err = nvs_get_i32(handle, "interval", &update_interval);
    if (err == ESP_OK) {
        btc_settings.update_interval = update_interval;
    }
    
    uint8_t auto_start;
    err = nvs_get_u8(handle, "autostart", &auto_start);
    if (err == ESP_OK) {
        btc_settings.auto_start = (auto_start != 0);
    }
    
    nvs_close(handle);
    
    return true;
}

// Save settings to NVS
bool WiFiConfigManager::saveSettings() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(nvs_namespace.c_str(), NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    // Save WiFi credentials
    err = nvs_set_str(handle, "wifi_ssid", wifi_credentials.ssid.c_str());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save WiFi SSID: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_set_str(handle, "wifi_pass", wifi_credentials.password.c_str());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save WiFi password: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    // Save BTC Clock settings
    err = nvs_set_str(handle, "currency", btc_settings.currency.c_str());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save currency: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_set_i32(handle, "interval", btc_settings.update_interval);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save update interval: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_set_u8(handle, "autostart", btc_settings.auto_start ? 1 : 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save auto-start setting: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Settings saved to NVS");
    return true;
}

// Start AP mode
bool WiFiConfigManager::startAP() {
    ESP_LOGI(TAG, "Starting AP mode with SSID: %s", ap_ssid.c_str());
    
    // Initialize WiFi in AP mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    
    // Configure AP
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.ap.ssid, ap_ssid.c_str(), sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = ap_ssid.length();
    
    if (!ap_password.empty()) {
        strncpy((char*)wifi_config.ap.password, ap_password.c_str(), sizeof(wifi_config.ap.password) - 1);
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    wifi_config.ap.max_connection = 4;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Start DNS server
    dns_server.start(53, "*", WiFi.softAPIP());
    
    return true;
}

// Start captive portal
bool WiFiConfigManager::startCaptivePortal() {
    ESP_LOGI(TAG, "Starting captive portal");
    
    // Configure HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    // Start HTTP server
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return false;
    }
    
    // Register URI handlers
    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handleRoot,
        .user_ctx = this
    };
    httpd_register_uri_handler(server, &uri_get);
    
    httpd_uri_t uri_scan = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = handleScan,
        .user_ctx = this
    };
    httpd_register_uri_handler(server, &uri_scan);
    
    httpd_uri_t uri_save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = handleSave,
        .user_ctx = this
    };
    httpd_register_uri_handler(server, &uri_save);
    
    // Register 404 handler
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, handleNotFound);
    
    return true;
}

// HTTP request handlers
esp_err_t WiFiConfigManager::handleRoot(httpd_req_t *req) {
    WiFiConfigManager* self = (WiFiConfigManager*)req->user_ctx;
    
    // Build HTML response
    std::string html = HTML_HEADER;
    html += HTML_WIFI_FORM;
    html += HTML_SETTINGS_FORM;
    html += HTML_FOOTER;
    
    // Replace placeholders
    std::string wifi_list = "<option value=\"\">Select WiFi Network</option>";
    // We'll populate this with actual scan results in the scan handler
    
    html.replace(html.find("{{WIFI_LIST}}"), 13, wifi_list);
    
    // Send response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    
    return ESP_OK;
}

esp_err_t WiFiConfigManager::handleScan(httpd_req_t *req) {
    WiFiConfigManager* self = (WiFiConfigManager*)req->user_ctx;
    
    // Scan for WiFi networks
    ESP_LOGI(TAG, "Scanning for WiFi networks");
    
    // Set WiFi to station mode for scanning
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    
    // Start scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };
    esp_wifi_scan_start(&scan_config, true);
    
    // Get scan results
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        ESP_LOGI(TAG, "No WiFi networks found");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "[]", 2);
        return ESP_OK;
    }
    
    // Allocate memory for scan results
    wifi_ap_record_t* ap_records = new wifi_ap_record_t[ap_count];
    if (!ap_records) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Get scan results
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    
    // Build JSON response
    std::string json = "[";
    for (int i = 0; i < ap_count; i++) {
        if (i > 0) {
            json += ",";
        }
        json += "\"" + std::string((char*)ap_records[i].ssid) + "\"";
    }
    json += "]";
    
    // Clean up
    delete[] ap_records;
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());
    
    // Switch back to AP mode
    esp_wifi_set_mode(WIFI_MODE_AP);
    
    return ESP_OK;
}

esp_err_t WiFiConfigManager::handleSave(httpd_req_t *req) {
    WiFiConfigManager* self = (WiFiConfigManager*)req->user_ctx;
    
    // Get content length
    int content_len = req->content_len;
    if (content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty request");
        return ESP_FAIL;
    }
    
    // Allocate memory for request data
    char* buf = new char[content_len + 1];
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Read request data
    int ret = httpd_req_recv(req, buf, content_len);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse form data
    std::string ssid;
    std::string password;
    std::string currency;
    int update_interval = 60;
    bool auto_start = true;
    
    // Very simple form data parsing
    std::string data(buf);
    size_t pos = 0;
    while (pos < data.length()) {
        size_t eq_pos = data.find('=', pos);
        if (eq_pos == std::string::npos) {
            break;
        }
        
        std::string key = data.substr(pos, eq_pos - pos);
        
        size_t next_pos = data.find('&', eq_pos);
        if (next_pos == std::string::npos) {
            next_pos = data.length();
        }
        
        std::string value = data.substr(eq_pos + 1, next_pos - eq_pos - 1);
        
        if (key == "ssid") {
            ssid = value;
        } else if (key == "password") {
            password = value;
        } else if (key == "currency") {
            currency = value;
        } else if (key == "interval") {
            try {
                update_interval = std::stoi(value);
            } catch (...) {
                update_interval = 60;
            }
        } else if (key == "autostart") {
            auto_start = (value == "on" || value == "true" || value == "1");
        }
        
        pos = next_pos + 1;
    }
    
    // Clean up
    delete[] buf;
    
    // Validate data
    if (ssid.empty()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID is required");
        return ESP_FAIL;
    }
    
    if (update_interval < 30 || update_interval > 3600) {
        update_interval = 60;
    }
    
    // Save settings
    self->setWiFiCredentials(ssid, password);
    self->setBTCClockSettings(currency, update_interval, auto_start);
    
    // Send response
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Configuration saved successfully. Please restart the device.", 58);
    
    return ESP_OK;
}

esp_err_t WiFiConfigManager::handleNotFound(httpd_req_t *req) {
    // Redirect all requests to the root page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    
    return ESP_OK;
}
