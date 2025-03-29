K5MarauderBTC_TODO.md
K5MarauderBTC Project To-Do List
Project Overview
A custom Bitcoin price tracker and clock integrated with ESP32 Marauder for the Waveshare ESP32-S3-LCD-1.47 board, controlled via Raspberry Pi 5 running Kali Linux.

Project Goals
Create a BTC price tracker that displays on the ESP32-S3-LCD-1.47
Integrate with ESP32 Marauder framework for pen testing capabilities
Enable control via Raspberry Pi 5 through USB or web interface
Implement auto-start BTC clock functionality on power-up
Develop easy WiFi configuration for potential commercial use
1. Repository Setup
[ ] Fork ESP32 Marauder repository to GitHub
[ ] Create new branch for BTC clock integration
[x] Set up proper .gitignore file
[x] Create comprehensive README.md
[ ] Add appropriate license information
2. Development Environment
[x] Install ESP-IDF in Windsurf
[x] Install ESP-IDF plugin
[x] Configure build settings for Waveshare ESP32-S3-LCD-1.47
[ ] Set up serial communication for debugging
3. Hardware Configuration
[ ] Configure display driver for Waveshare ESP32-S3-LCD-1.47
[x] Set up WiFi connectivity
[ ] Implement power management for battery efficiency
[ ] Test hardware compatibility with Marauder framework
[x] Configure auto-start functionality on power-up
4. BTC Clock Module Development
[x] Create basic module structure (BTC_Clock.h and BTC_Clock.cpp)
[x] Implement WiFi connection management
[x] Add Bitcoin price API integration
[x] Design clock UI with price, trends, and time display
[x] Implement price history tracking
[x] Create configurable update intervals
[x] Add multiple currency support
[x] Implement auto-start on power-up
5. Marauder Integration
[ ] Add BTC Clock to Marauder menu system
[ ] Implement "cloak mode" (Marauder disguised as BTC clock)
[ ] Create command system for switching modes
[ ] Ensure compatibility with existing Marauder features
[ ] Add BTC-themed icons and graphics
6. Control Interfaces
[x] Implement serial communication protocol for Pi control
[x] Create command-line interface for basic functions
[x] Develop web server for remote control
[x] Implement REST API endpoints
[x] Create simple web UI for control
[ ] Set up automatic connection on Pi boot
7. OTA Update System
[ ] Implement WiFi-based OTA update mechanism
[ ] Create update server configuration
[ ] Add version checking and notification system
[ ] Implement secure update process
[ ] Create web interface for updates
8. WiFi Configuration Portal
[ ] Implement captive portal for initial WiFi setup
[ ] Add configuration options for BTC API settings
[x] Create storage for saved settings
[ ] Implement reset mechanism for configuration
9. Testing & Optimization
[ ] Test WiFi connectivity and reliability
[ ] Optimize battery usage
[ ] Verify API reliability and implement fallbacks
[ ] Test integration with Marauder pen testing tools
[ ] Optimize memory usage
[ ] Test auto-start functionality
[ ] Perform stress testing
10. Documentation
[ ] Create installation guide
[ ] Write user manual
[ ] Document API endpoints
[ ] Create troubleshooting guide
[ ] Add development documentation for contributors
[ ] Document web interface and control commands
11. Deployment & Release
[ ] Create release package
[ ] Set up GitHub Actions for CI/CD
[ ] Create installation script for Raspberry Pi
[ ] Test full system integration
[ ] Document known issues and limitations
[ ] Create release tags on GitHub
Next Steps
1. Fork ESP32 Marauder repository to GitHub
2. Implement captive portal for initial WiFi setup
3. Add BTC Clock to Marauder menu system
4. Set up automatic connection on Pi boot
5. Implement secure OTA update mechanism

Resources
ESP32 Marauder GitHub
Waveshare ESP32-S3-LCD-1.47 Wiki
ESP-IDF Documentation
CoinGecko API Documentation
Notes
The ESP32-S3-LCD-1.47 has no touch functionality for this project
Control will be exclusively through Raspberry Pi 5 via USB or web interface
BTC clock should automatically activate on power-up
Consider security implications for a commercial product
