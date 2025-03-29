/**
 * @file dns_server.cpp
 * @brief Implementation of DNS Server for captive portal
 * @author K5MarauderBTC
 * @date 2025-03-29
 */

#include "dns_server.h"
#include <string.h>
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

// Static tag for logging
static const char* TAG = "DNS_SERVER";

// DNS header structure
struct DNSHeader {
    uint16_t id;
    uint8_t flags1;
    uint8_t flags2;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

// Constructor
DNSServer::DNSServer()
    : sock(-1),
      port(53),
      domain("*"),
      running(false)
{
    ip.addr = 0;
}

// Destructor
DNSServer::~DNSServer() {
    stop();
}

// Start the DNS server
bool DNSServer::start(uint16_t port_param, const std::string& domain_param, const ip4_addr_t& ip_param) {
    // Save parameters
    port = port_param;
    domain = domain_param;
    ip = ip_param;
    
    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        return false;
    }
    
    // Set socket options
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        ESP_LOGE(TAG, "Failed to set socket options: %d", errno);
        close(sock);
        sock = -1;
        return false;
    }
    
    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(sock);
        sock = -1;
        return false;
    }
    
    // Set non-blocking
    fcntl(sock, F_SETFL, O_NONBLOCK);
    
    running = true;
    ESP_LOGI(TAG, "DNS server started on port %d", port);
    
    return true;
}

// Stop the DNS server
void DNSServer::stop() {
    if (sock >= 0) {
        close(sock);
        sock = -1;
    }
    
    running = false;
    ESP_LOGI(TAG, "DNS server stopped");
}

// Process DNS requests
void DNSServer::processNextRequest() {
    if (!running || sock < 0) {
        return;
    }
    
    // Buffer for DNS packet
    uint8_t buffer[512];
    
    // Client address
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Receive packet
    int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
    if (len < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGE(TAG, "Failed to receive packet: %d", errno);
        }
        return;
    }
    
    // Process packet
    ip_addr_t remote_addr;
    remote_addr.type = IPADDR_TYPE_V4;
    remote_addr.u_addr.ip4.addr = client_addr.sin_addr.s_addr;
    
    processRequest(buffer, len, &remote_addr, ntohs(client_addr.sin_port));
}

// Process a DNS request
void DNSServer::processRequest(uint8_t* buffer, size_t len, ip_addr_t* remote_addr, uint16_t remote_port) {
    if (len < sizeof(DNSHeader)) {
        ESP_LOGE(TAG, "DNS packet too short");
        return;
    }
    
    // Parse DNS header
    DNSHeader* header = (DNSHeader*)buffer;
    
    // Check if it's a query
    if ((header->flags1 & 0x80) != 0) {
        ESP_LOGI(TAG, "Not a DNS query");
        return;
    }
    
    // Check if we have questions
    if (ntohs(header->qdcount) == 0) {
        ESP_LOGI(TAG, "No questions in DNS query");
        return;
    }
    
    // Parse domain name
    uint8_t* question = buffer + sizeof(DNSHeader);
    std::string qname;
    
    // Skip domain name
    uint8_t len_byte;
    while ((len_byte = *question++) != 0) {
        // Check for compression
        if ((len_byte & 0xC0) == 0xC0) {
            question++;
            break;
        }
        
        // Add dot if not first label
        if (!qname.empty()) {
            qname += ".";
        }
        
        // Add label
        for (int i = 0; i < len_byte; i++) {
            qname += (char)*question++;
        }
    }
    
    // Skip qtype and qclass
    question += 4;
    
    ESP_LOGI(TAG, "DNS query for domain: %s", qname.c_str());
    
    // Check if domain matches
    bool match = false;
    if (domain == "*") {
        match = true;
    } else if (qname == domain) {
        match = true;
    } else if (domain[0] == '.' && qname.length() > domain.length() &&
               qname.rfind(domain) == qname.length() - domain.length()) {
        match = true;
    }
    
    if (!match) {
        ESP_LOGI(TAG, "Domain does not match filter");
        return;
    }
    
    // Create response
    uint8_t response[512];
    memcpy(response, buffer, len);
    
    // Update header
    DNSHeader* response_header = (DNSHeader*)response;
    response_header->flags1 |= 0x80; // Set QR bit (response)
    response_header->flags2 |= 0x04; // Set AA bit (authoritative)
    response_header->ancount = htons(1); // One answer
    
    // Add answer
    uint8_t* answer = response + (question - buffer);
    
    // Add pointer to domain name
    *answer++ = 0xC0; // Compression pointer
    *answer++ = 0x0C; // Pointer to domain name at offset 12 (after header)
    
    // Add type (A record)
    *answer++ = 0x00;
    *answer++ = 0x01;
    
    // Add class (IN)
    *answer++ = 0x00;
    *answer++ = 0x01;
    
    // Add TTL (60 seconds)
    *answer++ = 0x00;
    *answer++ = 0x00;
    *answer++ = 0x00;
    *answer++ = 0x3C;
    
    // Add data length (4 bytes for IPv4)
    *answer++ = 0x00;
    *answer++ = 0x04;
    
    // Add IP address
    *answer++ = ip4_addr1(&ip);
    *answer++ = ip4_addr2(&ip);
    *answer++ = ip4_addr3(&ip);
    *answer++ = ip4_addr4(&ip);
    
    // Calculate response length
    size_t response_len = answer - response;
    
    // Send response
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = remote_addr->u_addr.ip4.addr;
    dest_addr.sin_port = htons(remote_port);
    
    int sent = sendto(sock, response, response_len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send DNS response: %d", errno);
    } else {
        ESP_LOGI(TAG, "Sent DNS response for %s to %s", qname.c_str(), ipaddr_ntoa(remote_addr));
    }
}
