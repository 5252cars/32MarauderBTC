/**
 * @file dns_server.h
 * @brief DNS Server for captive portal
 * @author K5MarauderBTC
 * @date 2025-03-29
 */

#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <string>
#include "esp_netif.h"
#include "lwip/ip4_addr.h"

/**
 * @brief Simple DNS Server for captive portal
 * 
 * This class implements a simple DNS server that responds to all DNS queries
 * with the IP address of the ESP32, enabling a captive portal.
 */
class DNSServer {
private:
    int sock;
    uint16_t port;
    std::string domain;
    ip4_addr_t ip;
    bool running;
    
    void parsePacket(uint8_t* buffer, size_t len);
    void processRequest(uint8_t* buffer, size_t len, ip_addr_t* remote_addr, uint16_t remote_port);
    
public:
    /**
     * @brief Constructor
     */
    DNSServer();
    
    /**
     * @brief Destructor
     */
    ~DNSServer();
    
    /**
     * @brief Start the DNS server
     * 
     * @param port_param Port to listen on (default: 53)
     * @param domain_param Domain to match (default: "*" for all domains)
     * @param ip_param IP address to respond with
     * @return true if started successfully, false otherwise
     */
    bool start(uint16_t port_param = 53, const std::string& domain_param = "*", const ip4_addr_t& ip_param = {});
    
    /**
     * @brief Stop the DNS server
     */
    void stop();
    
    /**
     * @brief Process DNS requests
     * 
     * This function should be called regularly to process DNS requests
     */
    void processNextRequest();
};

#endif // DNS_SERVER_H
