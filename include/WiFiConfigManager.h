/**
 * @file WiFiConfigManager.h
 * @brief WiFi Configuration Manager for K5MarauderBTC
 * @author K5MarauderBTC
 * @date 2025-03-29
 */

#ifndef WIFI_CONFIG_MANAGER_H
#define WIFI_CONFIG_MANAGER_H

#include <string>
#include <vector>
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"

/**
 * @brief Structure to hold WiFi credentials
 */
struct WiFiCredentials {
    std::string ssid;
    std::string password;
};

/**
 * @brief Structure to hold BTC Clock settings
 */
struct BTCClockSettings {
    std::string currency;
    int update_interval;
    bool auto_start;
};

/**
 * @brief Class for WiFi Configuration Management
 * 
 * This class handles WiFi configuration, including a captive portal for
 * initial setup and storage of WiFi credentials and other settings.
 */
class WiFiConfigManager {
private:
    // WiFi credentials
    WiFiCredentials wifi_credentials;
    
    // BTC Clock settings
    BTCClockSettings btc_settings;
    
    // HTTP server for captive portal
    httpd_handle_t server;
    
    // AP mode SSID and password
    std::string ap_ssid;
    std::string ap_password;
    
    // NVS namespace for storing settings
    std::string nvs_namespace;
    
    // Flag to indicate if WiFi is configured
    bool wifi_configured;
    
    // Flag to indicate if captive portal is active
    bool captive_portal_active;
    
    // Internal methods
    bool loadSettings();
    bool saveSettings();
    bool startAP();
    bool startCaptivePortal();
    void stopCaptivePortal();
    
    // HTTP request handlers
    static esp_err_t handleRoot(httpd_req_t *req);
    static esp_err_t handleSave(httpd_req_t *req);
    static esp_err_t handleScan(httpd_req_t *req);
    static esp_err_t handleNotFound(httpd_req_t *req);
    
public:
    /**
     * @brief Constructor
     * 
     * @param ap_ssid SSID to use for AP mode
     * @param ap_password Password to use for AP mode (empty for open network)
     * @param nvs_namespace Namespace to use for NVS storage
     */
    WiFiConfigManager(const std::string& ap_ssid = "K5MarauderBTC",
                      const std::string& ap_password = "",
                      const std::string& nvs_namespace = "btc_config");
    
    /**
     * @brief Destructor
     */
    ~WiFiConfigManager();
    
    /**
     * @brief Initialize the WiFi Configuration Manager
     * 
     * @return true if initialization successful, false otherwise
     */
    bool init();
    
    /**
     * @brief Check if WiFi is configured
     * 
     * @return true if WiFi is configured, false otherwise
     */
    bool isWiFiConfigured() const;
    
    /**
     * @brief Start the captive portal for WiFi configuration
     * 
     * @return true if captive portal started successfully, false otherwise
     */
    bool startConfigPortal();
    
    /**
     * @brief Stop the captive portal
     */
    void stopConfigPortal();
    
    /**
     * @brief Connect to WiFi using stored credentials
     * 
     * @return true if connection successful, false otherwise
     */
    bool connectToWiFi();
    
    /**
     * @brief Get the WiFi credentials
     * 
     * @return The WiFi credentials
     */
    const WiFiCredentials& getWiFiCredentials() const;
    
    /**
     * @brief Get the BTC Clock settings
     * 
     * @return The BTC Clock settings
     */
    const BTCClockSettings& getBTCClockSettings() const;
    
    /**
     * @brief Set the WiFi credentials
     * 
     * @param ssid The WiFi SSID
     * @param password The WiFi password
     * @return true if successful, false otherwise
     */
    bool setWiFiCredentials(const std::string& ssid, const std::string& password);
    
    /**
     * @brief Set the BTC Clock settings
     * 
     * @param currency The currency to use
     * @param update_interval The update interval in seconds
     * @param auto_start Whether to auto-start the BTC Clock
     * @return true if successful, false otherwise
     */
    bool setBTCClockSettings(const std::string& currency, int update_interval, bool auto_start);
    
    /**
     * @brief Reset all settings to defaults
     * 
     * @return true if successful, false otherwise
     */
    bool resetSettings();
    
    /**
     * @brief Process captive portal requests
     * 
     * This function should be called regularly to process captive portal requests
     */
    void process();
};

#endif // WIFI_CONFIG_MANAGER_H
