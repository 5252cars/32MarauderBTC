/**
 * @file main.cpp
 * @brief Main application for K5MarauderBTC
 * @author K5MarauderBTC
 * @date 2025-03-29
 */

#include <stdio.h>
#include <string>
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "BTC_Clock.h"
#include "WiFiConfigManager.h"

// Static tag for logging
static const char* TAG = "K5MarauderBTC";

// Currency symbols
static std::map<std::string, std::string> currency_symbols = {
    {"usd", "$"},
    {"eur", "€"},
    {"gbp", "£"},
    {"jpy", "¥"},
    {"cad", "C$"},
    {"aud", "A$"},
    {"chf", "CHF"},
    {"cny", "¥"},
    {"krw", "₩"},
    {"rub", "₽"}
};

// Display class placeholder (will be replaced with actual Marauder display class)
class Display {
public:
    void clearScreen() {
        ESP_LOGI(TAG, "Clearing screen");
    }
    
    void drawText(int x, int y, const char* text, int color) {
        ESP_LOGI(TAG, "Drawing text at (%d,%d): %s", x, y, text);
    }
    
    void drawRect(int x, int y, int w, int h, int color) {
        ESP_LOGI(TAG, "Drawing rectangle at (%d,%d) with size (%d,%d)", x, y, w, h);
    }
    
    void drawLine(int x1, int y1, int x2, int y2, int color) {
        ESP_LOGI(TAG, "Drawing line from (%d,%d) to (%d,%d)", x1, y1, x2, y2);
    }
};

// BTC Clock instance
BTC_Clock btc_clock;

// Display instance
Display display;

// Current currency
std::string current_currency = "usd";

// Task to run BTC Clock
void btc_clock_task(void* pvParameters) {
    while (1) {
        btc_clock.main();
        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz refresh rate
    }
}

// Function to handle serial commands
void handle_serial_command(const char* command) {
    ESP_LOGI(TAG, "Received command: %s", command);
    
    // Parse command
    std::string cmd(command);
    
    if (cmd.find("set_currency") == 0) {
        // Extract currency code
        size_t pos = cmd.find(" ");
        if (pos != std::string::npos) {
            std::string currency = cmd.substr(pos + 1);
            
            // Check if currency is supported
            if (currency_symbols.find(currency) != currency_symbols.end()) {
                ESP_LOGI(TAG, "Setting currency to %s", currency.c_str());
                current_currency = currency;
                btc_clock.setCurrency(currency);
            } else {
                ESP_LOGE(TAG, "Unsupported currency: %s", currency.c_str());
            }
        }
    } else if (cmd.find("set_interval") == 0) {
        // Extract interval value
        size_t pos = cmd.find(" ");
        if (pos != std::string::npos) {
            std::string interval_str = cmd.substr(pos + 1);
            try {
                int interval = std::stoi(interval_str);
                if (interval >= 30 && interval <= 3600) {
                    ESP_LOGI(TAG, "Setting update interval to %d seconds", interval);
                    btc_clock.setUpdateInterval(interval * 1000); // Convert to ms
                } else {
                    ESP_LOGE(TAG, "Invalid interval: %d (must be between 30 and 3600 seconds)", interval);
                }
            } catch (const std::exception& e) {
                ESP_LOGE(TAG, "Invalid interval format: %s", interval_str.c_str());
            }
        }
    } else if (cmd == "restart") {
        ESP_LOGI(TAG, "Restarting device...");
        esp_restart();
    } else if (cmd == "refresh") {
        ESP_LOGI(TAG, "Refreshing data...");
        // Force a refresh by setting last_update to 0
        // This is a bit of a hack, but it works for now
        btc_clock.setUpdateInterval(0);
        vTaskDelay(pdMS_TO_TICKS(100));
        btc_clock.setUpdateInterval(60000); // Reset to default
    } else if (cmd == "status") {
        // Return status as JSON
        char status[256];
        snprintf(status, sizeof(status),
                 "{\"mode\":\"btc\",\"btc_price\":%.2f,\"btc_change\":%.2f,\"wifi_connected\":%s,\"last_update\":%lu,\"currency\":\"%s\"}",
                 btc_clock.getCurrentPrice(),
                 btc_clock.get24hChange(),
                 btc_clock.getCurrentPrice() > 0 ? "true" : "false",
                 esp_timer_get_time() / 1000000, // Convert to seconds
                 current_currency.c_str());
        
        ESP_LOGI(TAG, "Status: %s", status);
        // In a real implementation, we would send this back over serial
    } else if (cmd == "start_config_portal") {
        ESP_LOGI(TAG, "Starting WiFi configuration portal");
        if (btc_clock.startWiFiConfigPortal()) {
            ESP_LOGI(TAG, "WiFi configuration portal started");
        } else {
            ESP_LOGE(TAG, "Failed to start WiFi configuration portal");
        }
    } else if (cmd == "stop_config_portal") {
        ESP_LOGI(TAG, "Stopping WiFi configuration portal");
        btc_clock.stopWiFiConfigPortal();
    } else if (cmd == "reset_wifi_config") {
        ESP_LOGI(TAG, "Resetting WiFi configuration");
        if (btc_clock.resetWiFiConfig()) {
            ESP_LOGI(TAG, "WiFi configuration reset successfully");
        } else {
            ESP_LOGE(TAG, "Failed to reset WiFi configuration");
        }
    } else if (cmd == "wifi_status") {
        // Return WiFi status as JSON
        char status[128];
        snprintf(status, sizeof(status),
                 "{\"wifi_configured\":%s}",
                 btc_clock.isWiFiConfigured() ? "true" : "false");
        
        ESP_LOGI(TAG, "WiFi Status: %s", status);
        // In a real implementation, we would send this back over serial
    } else {
        ESP_LOGE(TAG, "Unknown command: %s", command);
    }
}

// Task to handle serial input
void serial_task(void* pvParameters) {
    char buffer[128];
    int idx = 0;
    
    while (1) {
        // In a real implementation, we would read from UART
        // For now, just simulate some commands
        
        // Simulate receiving "status" command every 5 seconds
        handle_serial_command("status");
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "K5MarauderBTC starting up");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Print chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is %s chip with %d CPU cores, WiFi%s%s, silicon revision %d",
             CONFIG_IDF_TARGET,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
             chip_info.revision);
    ESP_LOGI(TAG, "%dMB %s flash", spi_flash_get_chip_size() / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    
    // Initialize BTC Clock
    if (btc_clock.init(&display)) {
        ESP_LOGI(TAG, "BTC Clock initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize BTC Clock");
        return;
    }
    
    // Set default currency
    btc_clock.setCurrency(current_currency);
    
    // Start BTC Clock
    btc_clock.start();
    
    // Create BTC Clock task
    xTaskCreate(btc_clock_task, "btc_clock_task", 4096, NULL, 5, NULL);
    
    // Create serial task
    xTaskCreate(serial_task, "serial_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "K5MarauderBTC startup complete");
}
