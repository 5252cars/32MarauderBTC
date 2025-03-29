/**
 * @file BTC_Clock.h
 * @brief Header file for the Bitcoin Clock module for ESP32 Marauder
 * @author K5MarauderBTC
 * @date 2025-03-29
 */

#ifndef BTC_CLOCK_H
#define BTC_CLOCK_H

#include <string>
#include <vector>
#include "esp_http_client.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "WiFiConfigManager.h"

// Forward declarations
class Display;

// Structure to hold price data point
struct PriceDataPoint {
    unsigned long timestamp;
    float price;
};

/**
 * @brief Class for Bitcoin Clock functionality
 * 
 * This class handles fetching Bitcoin price data, displaying it on the screen,
 * and managing the clock functionality.
 */
class BTC_Clock {
private:
    // Display reference
    Display* display;
    
    // Bitcoin price data
    float btc_price;
    float btc_change_24h;
    bool price_up;
    
    // Price history
    std::vector<PriceDataPoint> price_history;
    size_t max_history_size;
    
    // Time tracking
    unsigned long last_update;
    unsigned long update_interval;
    
    // WiFi connection status
    bool wifi_connected;
    
    // API endpoint
    std::string api_url;
    
    // Currency
    std::string currency;
    
    // WiFi Configuration Manager
    WiFiConfigManager* wifi_config_manager;
    
    // Internal methods
    bool fetchBTCPrice();
    void displayPrice();
    bool connectToWiFi();
    void updatePriceHistory();
    void displayPriceHistory();
    bool startConfigPortal();
    
public:
    /**
     * @brief Constructor
     */
    BTC_Clock();
    
    /**
     * @brief Destructor
     */
    ~BTC_Clock();
    
    /**
     * @brief Initialize the BTC Clock module
     * 
     * @param display_obj Pointer to the display object
     * @return true if initialization successful, false otherwise
     */
    bool init(Display* display_obj);
    
    /**
     * @brief Main loop function
     * 
     * This function should be called regularly to update the display
     * and fetch new price data when needed.
     */
    void main();
    
    /**
     * @brief Set the API URL
     * 
     * @param url The API URL to use for fetching Bitcoin price data
     */
    void setApiUrl(const std::string& url);
    
    /**
     * @brief Set the update interval
     * 
     * @param interval_ms The interval in milliseconds between price updates
     */
    void setUpdateInterval(unsigned long interval_ms);
    
    /**
     * @brief Set the currency
     * 
     * @param curr The currency to use (e.g., "usd", "eur", "gbp")
     */
    void setCurrency(const std::string& curr);
    
    /**
     * @brief Get the current Bitcoin price
     * 
     * @return The current Bitcoin price
     */
    float getCurrentPrice() const;
    
    /**
     * @brief Get the 24-hour price change
     * 
     * @return The 24-hour price change as a percentage
     */
    float get24hChange() const;
    
    /**
     * @brief Get the price history
     * 
     * @return Vector of price history data points
     */
    const std::vector<PriceDataPoint>& getPriceHistory() const;
    
    /**
     * @brief Set the maximum history size
     * 
     * @param size Maximum number of price data points to store
     */
    void setMaxHistorySize(size_t size);
    
    /**
     * @brief Start the BTC Clock
     * 
     * This function starts the BTC Clock and displays it on the screen.
     */
    void start();
    
    /**
     * @brief Stop the BTC Clock
     * 
     * This function stops the BTC Clock and clears the display.
     */
    void stop();
    
    /**
     * @brief Check if WiFi is configured
     * 
     * @return true if WiFi is configured, false otherwise
     */
    bool isWiFiConfigured() const;
    
    /**
     * @brief Start the WiFi configuration portal
     * 
     * This function starts the WiFi configuration portal, allowing the user
     * to configure WiFi credentials and other settings.
     * 
     * @return true if the portal was started successfully, false otherwise
     */
    bool startWiFiConfigPortal();
    
    /**
     * @brief Stop the WiFi configuration portal
     * 
     * This function stops the WiFi configuration portal.
     */
    void stopWiFiConfigPortal();
    
    /**
     * @brief Process WiFi configuration portal requests
     * 
     * This function should be called regularly to process WiFi configuration
     * portal requests.
     */
    void processWiFiConfigPortal();
    
    /**
     * @brief Reset WiFi configuration
     * 
     * This function resets the WiFi configuration to defaults.
     * 
     * @return true if successful, false otherwise
     */
    bool resetWiFiConfig();
};

#endif // BTC_CLOCK_H
