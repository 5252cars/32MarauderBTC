/**
 * @file BTC_Clock.cpp
 * @brief Implementation of the Bitcoin Clock module for ESP32 Marauder
 * @author K5MarauderBTC
 * @date 2025-03-29
 */

#include "BTC_Clock.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "cJSON.h"

// Assuming Display class will be available from Marauder framework
// This is a placeholder until we integrate with the actual Marauder code
class Display {
public:
    void clearScreen() {}
    void drawText(int x, int y, const char* text, int color) {}
    void drawRect(int x, int y, int w, int h, int color) {}
    void drawLine(int x1, int y1, int x2, int y2, int color) {}
};

// Static tag for logging
static const char* TAG = "BTC_CLOCK";

// Constructor
BTC_Clock::BTC_Clock() :
    display(nullptr),
    btc_price(0.0f),
    btc_change_24h(0.0f),
    price_up(true),
    max_history_size(24), // Default to 24 data points (24 hours if updated hourly)
    last_update(0),
    update_interval(60000), // Default to 1 minute
    wifi_connected(false),
    api_url("https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd&include_24hr_change=true"),
    currency("usd"),
    wifi_config_manager(nullptr)
{
    ESP_LOGI(TAG, "BTC Clock module created");
}

// Destructor
BTC_Clock::~BTC_Clock() {
    ESP_LOGI(TAG, "BTC Clock module destroyed");
    
    // Clean up WiFi Configuration Manager
    if (wifi_config_manager) {
        delete wifi_config_manager;
        wifi_config_manager = nullptr;
    }
}

// Initialize the BTC Clock module
bool BTC_Clock::init(Display* display_obj) {
    ESP_LOGI(TAG, "Initializing BTC Clock module");
    
    // Store display reference
    display = display_obj;
    
    // Initialize NVS for storing settings
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi Configuration Manager
    wifi_config_manager = new WiFiConfigManager("K5MarauderBTC", "", "btc_config");
    if (!wifi_config_manager->init()) {
        ESP_LOGE(TAG, "Failed to initialize WiFi Configuration Manager");
        return false;
    }
    
    // Check if WiFi is configured
    if (wifi_config_manager->isWiFiConfigured()) {
        // Connect to WiFi
        wifi_connected = connectToWiFi();
        
        // Fetch initial price data
        if (wifi_connected) {
            if (fetchBTCPrice()) {
                ESP_LOGI(TAG, "Initial BTC price fetched: %.2f", btc_price);
                
                // Add initial data point to history
                updatePriceHistory();
            } else {
                ESP_LOGE(TAG, "Failed to fetch initial BTC price");
            }
        }
    } else {
        // Start WiFi configuration portal
        ESP_LOGI(TAG, "WiFi not configured, starting configuration portal");
        startWiFiConfigPortal();
    }
    
    return true;
}

// Main loop function
void BTC_Clock::main() {
    // Process WiFi configuration portal requests
    processWiFiConfigPortal();
    
    // Check if it's time to update the price
    unsigned long current_time = esp_timer_get_time() / 1000; // Convert to ms
    if (current_time - last_update > update_interval) {
        if (wifi_connected) {
            if (fetchBTCPrice()) {
                ESP_LOGI(TAG, "BTC price updated: %.2f", btc_price);
                
                // Update price history
                updatePriceHistory();
            } else {
                ESP_LOGE(TAG, "Failed to update BTC price");
            }
        } else {
            // Try to reconnect to WiFi
            wifi_connected = connectToWiFi();
        }
    }
    
    // Display the price
    displayPrice();
}

// Set the API URL
void BTC_Clock::setApiUrl(const std::string& url) {
    api_url = url;
    ESP_LOGI(TAG, "API URL set to: %s", api_url.c_str());
}

// Set the update interval
void BTC_Clock::setUpdateInterval(unsigned long interval_ms) {
    update_interval = interval_ms;
    ESP_LOGI(TAG, "Update interval set to: %lu ms", update_interval);
}

// Set the currency
void BTC_Clock::setCurrency(const std::string& curr) {
    currency = curr;
    
    // Update the API URL to use the new currency
    api_url = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=" + 
              currency + "&include_24hr_change=true";
    
    ESP_LOGI(TAG, "Currency set to: %s", currency.c_str());
    ESP_LOGI(TAG, "API URL updated to: %s", api_url.c_str());
}

// Get the current Bitcoin price
float BTC_Clock::getCurrentPrice() const {
    return btc_price;
}

// Get the 24-hour price change
float BTC_Clock::get24hChange() const {
    return btc_change_24h;
}

// Get the price history
const std::vector<PriceDataPoint>& BTC_Clock::getPriceHistory() const {
    return price_history;
}

// Set the maximum history size
void BTC_Clock::setMaxHistorySize(size_t size) {
    max_history_size = size;
    
    // If current history is larger than new max size, trim it
    if (price_history.size() > max_history_size) {
        // Remove oldest entries to match new max size
        price_history.erase(price_history.begin(), price_history.begin() + (price_history.size() - max_history_size));
    }
    
    ESP_LOGI(TAG, "Max history size set to: %zu", max_history_size);
}

// Start the BTC Clock
void BTC_Clock::start() {
    ESP_LOGI(TAG, "Starting BTC Clock");
    
    // Clear the display
    if (display) {
        display->clearScreen();
    }
    
    // Fetch the latest price
    if (wifi_connected) {
        fetchBTCPrice();
        updatePriceHistory();
    } else {
        // Try to connect to WiFi
        wifi_connected = connectToWiFi();
        if (wifi_connected) {
            fetchBTCPrice();
            updatePriceHistory();
        }
    }
    
    // Display the price
    displayPrice();
}

// Stop the BTC Clock
void BTC_Clock::stop() {
    ESP_LOGI(TAG, "Stopping BTC Clock");
    
    // Clear the display
    if (display) {
        display->clearScreen();
    }
}

// Check if WiFi is configured
bool BTC_Clock::isWiFiConfigured() const {
    if (wifi_config_manager) {
        return wifi_config_manager->isWiFiConfigured();
    }
    return false;
}

// Start the WiFi configuration portal
bool BTC_Clock::startWiFiConfigPortal() {
    ESP_LOGI(TAG, "Starting WiFi configuration portal");
    
    if (wifi_config_manager) {
        return wifi_config_manager->startConfigPortal();
    }
    
    return false;
}

// Stop the WiFi configuration portal
void BTC_Clock::stopWiFiConfigPortal() {
    ESP_LOGI(TAG, "Stopping WiFi configuration portal");
    
    if (wifi_config_manager) {
        wifi_config_manager->stopConfigPortal();
    }
}

// Process WiFi configuration portal requests
void BTC_Clock::processWiFiConfigPortal() {
    if (wifi_config_manager) {
        wifi_config_manager->process();
    }
}

// Reset WiFi configuration
bool BTC_Clock::resetWiFiConfig() {
    ESP_LOGI(TAG, "Resetting WiFi configuration");
    
    if (wifi_config_manager) {
        return wifi_config_manager->resetSettings();
    }
    
    return false;
}

// Fetch Bitcoin price from API
bool BTC_Clock::fetchBTCPrice() {
    ESP_LOGI(TAG, "Fetching BTC price from API");
    
    // This is a placeholder for the actual HTTP request implementation
    // In a real implementation, we would use esp_http_client to make a request to the API
    
    // For now, just simulate a successful response
    btc_price = 60000.0f + (rand() % 5000 - 2500) / 100.0f;
    btc_change_24h = (rand() % 1000 - 500) / 100.0f;
    price_up = btc_change_24h >= 0;
    
    last_update = esp_timer_get_time() / 1000; // Convert to ms
    
    return true;
}

// Connect to WiFi
bool BTC_Clock::connectToWiFi() {
    ESP_LOGI(TAG, "Connecting to WiFi");
    
    if (wifi_config_manager && wifi_config_manager->isWiFiConfigured()) {
        return wifi_config_manager->connectToWiFi();
    }
    
    return false;
}

// Update price history
void BTC_Clock::updatePriceHistory() {
    ESP_LOGI(TAG, "Updating price history");
    
    // Create a new data point
    PriceDataPoint dataPoint;
    dataPoint.timestamp = esp_timer_get_time() / 1000; // Convert to ms
    dataPoint.price = btc_price;
    
    // Add to history
    price_history.push_back(dataPoint);
    
    // If history is too large, remove oldest entry
    if (price_history.size() > max_history_size) {
        price_history.erase(price_history.begin());
    }
    
    ESP_LOGI(TAG, "Price history updated, now contains %zu data points", price_history.size());
}

// Display price history
void BTC_Clock::displayPriceHistory() {
    if (!display || price_history.empty()) {
        return;
    }
    
    ESP_LOGI(TAG, "Displaying price history graph");
    
    // This is a placeholder for the actual graph drawing implementation
    // In a real implementation, we would draw a line graph of the price history
    
    // Find min and max prices for scaling
    float min_price = price_history[0].price;
    float max_price = price_history[0].price;
    
    for (const auto& dataPoint : price_history) {
        if (dataPoint.price < min_price) {
            min_price = dataPoint.price;
        }
        if (dataPoint.price > max_price) {
            max_price = dataPoint.price;
        }
    }
    
    // Add a 5% margin to min and max
    float price_range = max_price - min_price;
    min_price -= price_range * 0.05f;
    max_price += price_range * 0.05f;
    
    // Calculate graph dimensions and position
    // This would be replaced with actual display dimensions
    int graph_x = 10;
    int graph_y = 80;
    int graph_width = 152; // Assuming display width is 172
    int graph_height = 100;
    
    // Draw graph axes
    // display->drawLine(graph_x, graph_y, graph_x, graph_y + graph_height, 1); // Y-axis
    // display->drawLine(graph_x, graph_y + graph_height, graph_x + graph_width, graph_y + graph_height, 1); // X-axis
    
    // Draw price points and connect with lines
    int prev_x = 0;
    int prev_y = 0;
    
    for (size_t i = 0; i < price_history.size(); i++) {
        // Calculate x and y coordinates
        int x = graph_x + (i * graph_width) / (price_history.size() - 1);
        int y = graph_y + graph_height - ((price_history[i].price - min_price) * graph_height) / (max_price - min_price);
        
        // Draw point
        // display->drawRect(x - 1, y - 1, 3, 3, 1);
        
        // Connect with line to previous point
        if (i > 0) {
            // display->drawLine(prev_x, prev_y, x, y, 1);
        }
        
        prev_x = x;
        prev_y = y;
    }
    
    ESP_LOGI(TAG, "Price history graph displayed");
}

// Display the price on the screen
void BTC_Clock::displayPrice() {
    if (!display) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    
    // Clear the screen
    display->clearScreen();
    
    // This is a placeholder for the actual display implementation
    // In a real implementation, we would use the display object to draw text and graphics
    
    ESP_LOGI(TAG, "Displaying BTC price: %.2f (%.2f%%)", btc_price, btc_change_24h);
    
    // Display price history graph
    displayPriceHistory();
}
