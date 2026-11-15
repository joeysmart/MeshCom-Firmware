#pragma once

#include <Arduino.h>

#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

class RpiUdp
{
public:
    int begin(uint16_t port);
    void stop();
    int beginPacket(IPAddress address, uint16_t port);
    size_t write(const uint8_t *data, size_t length);
    int endPacket();
    int parsePacket();
    int read(uint8_t *buffer, size_t length);
    void flush();

private:
    int m_socket = -1;
    struct sockaddr_in m_destination = {};
    bool m_destinationValid = false;
    bool m_lastSendSucceeded = false;
    uint8_t m_receiveBuffer[2048] = {};
    size_t m_receiveLength = 0;
    size_t m_receiveOffset = 0;
};

bool rpiResolveUdpPeer(const char *host, uint16_t port,
                      IPAddress &peer, IPAddress &local, IPAddress &netmask,
                      char *error, size_t errorCapacity);
