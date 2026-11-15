#include "rpi_udp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
void setAddress(IPAddress &address, uint32_t networkOrder)
{
    const uint32_t hostOrder = ntohl(networkOrder);
    address = IPAddress(static_cast<uint8_t>(hostOrder >> 24),
                        static_cast<uint8_t>(hostOrder >> 16),
                        static_cast<uint8_t>(hostOrder >> 8),
                        static_cast<uint8_t>(hostOrder));
}

void setError(char *error, size_t capacity, const char *message)
{
    if (error != nullptr && capacity > 0)
        snprintf(error, capacity, "%s", message);
}
}

int RpiUdp::begin(uint16_t port)
{
    stop();

    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0)
        return 0;

    int reuse = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        stop();
        return 0;
    }

    const int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags < 0 || fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        stop();
        return 0;
    }

    struct sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);
    if (bind(m_socket, reinterpret_cast<struct sockaddr *>(&local), sizeof(local)) < 0)
    {
        stop();
        return 0;
    }

    return 1;
}

void RpiUdp::stop()
{
    if (m_socket >= 0)
    {
        close(m_socket);
        m_socket = -1;
    }
    m_destinationValid = false;
    m_lastSendSucceeded = false;
    m_receiveLength = 0;
    m_receiveOffset = 0;
}

int RpiUdp::beginPacket(IPAddress address, uint16_t port)
{
    if (m_socket < 0)
        return 0;

    const uint32_t hostOrder =
        (static_cast<uint32_t>(address[0]) << 24) |
        (static_cast<uint32_t>(address[1]) << 16) |
        (static_cast<uint32_t>(address[2]) << 8) |
        static_cast<uint32_t>(address[3]);
    m_destination = {};
    m_destination.sin_family = AF_INET;
    m_destination.sin_addr.s_addr = htonl(hostOrder);
    m_destination.sin_port = htons(port);
    m_destinationValid = true;
    m_lastSendSucceeded = false;
    return 1;
}

size_t RpiUdp::write(const uint8_t *data, size_t length)
{
    if (m_socket < 0 || !m_destinationValid || data == nullptr || length == 0)
        return 0;

    const ssize_t sent = sendto(m_socket, data, length, 0,
                                reinterpret_cast<struct sockaddr *>(&m_destination),
                                sizeof(m_destination));
    m_lastSendSucceeded = sent >= 0 && static_cast<size_t>(sent) == length;
    if (!m_lastSendSucceeded)
        Serial.printf("[EXT] sendto_failed;errno;%d\n", errno);
    return m_lastSendSucceeded ? length : 0;
}

int RpiUdp::endPacket()
{
    const int result = m_lastSendSucceeded ? 1 : 0;
    m_destinationValid = false;
    m_lastSendSucceeded = false;
    return result;
}

int RpiUdp::parsePacket()
{
    if (m_socket < 0)
        return 0;

    struct sockaddr_in sender = {};
    socklen_t senderLength = sizeof(sender);
    const ssize_t received = recvfrom(m_socket, m_receiveBuffer, sizeof(m_receiveBuffer),
                                      MSG_DONTWAIT | MSG_TRUNC,
                                      reinterpret_cast<struct sockaddr *>(&sender),
                                      &senderLength);
    if (received < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            Serial.printf("[EXT] recvfrom_failed;errno;%d\n", errno);
        return 0;
    }

    m_receiveLength = static_cast<size_t>(received);
    if (m_receiveLength > sizeof(m_receiveBuffer))
        m_receiveLength = sizeof(m_receiveBuffer);
    m_receiveOffset = 0;
    if (received > INT_MAX)
        return INT_MAX;
    return static_cast<int>(received);
}

int RpiUdp::read(uint8_t *buffer, size_t length)
{
    if (buffer == nullptr || length == 0 || m_receiveOffset >= m_receiveLength)
        return 0;

    const size_t available = m_receiveLength - m_receiveOffset;
    const size_t count = (length < available) ? length : available;
    memcpy(buffer, m_receiveBuffer + m_receiveOffset, count);
    m_receiveOffset += count;
    return static_cast<int>(count);
}

void RpiUdp::flush()
{
    m_receiveOffset = m_receiveLength;
}

bool rpiResolveUdpPeer(const char *host, uint16_t port,
                       IPAddress &peer, IPAddress &local, IPAddress &netmask,
                       char *error, size_t errorCapacity)
{
    if (host == nullptr || host[0] == '\0')
    {
        setError(error, errorCapacity, "empty peer host");
        return false;
    }

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *addresses = nullptr;
    const int resolveResult = getaddrinfo(host, nullptr, &hints, &addresses);
    if (resolveResult != 0 || addresses == nullptr)
    {
        setError(error, errorCapacity, resolveResult == 0 ? "no IPv4 address" : gai_strerror(resolveResult));
        if (addresses != nullptr)
            freeaddrinfo(addresses);
        return false;
    }

    struct sockaddr_in target = *reinterpret_cast<struct sockaddr_in *>(addresses->ai_addr);
    freeaddrinfo(addresses);
    target.sin_port = htons(port);
    setAddress(peer, target.sin_addr.s_addr);

    const int probe = socket(AF_INET, SOCK_DGRAM, 0);
    if (probe < 0)
    {
        setError(error, errorCapacity, strerror(errno));
        return false;
    }

    if (connect(probe, reinterpret_cast<struct sockaddr *>(&target), sizeof(target)) < 0)
    {
        const int savedError = errno;
        close(probe);
        setError(error, errorCapacity, strerror(savedError));
        return false;
    }

    struct sockaddr_in source = {};
    socklen_t sourceLength = sizeof(source);
    if (getsockname(probe, reinterpret_cast<struct sockaddr *>(&source), &sourceLength) < 0)
    {
        const int savedError = errno;
        close(probe);
        setError(error, errorCapacity, strerror(savedError));
        return false;
    }
    close(probe);
    setAddress(local, source.sin_addr.s_addr);

    struct ifaddrs *interfaces = nullptr;
    if (getifaddrs(&interfaces) < 0)
    {
        setError(error, errorCapacity, strerror(errno));
        return false;
    }

    bool foundNetmask = false;
    for (const struct ifaddrs *entry = interfaces; entry != nullptr; entry = entry->ifa_next)
    {
        if (entry->ifa_addr == nullptr || entry->ifa_netmask == nullptr ||
            entry->ifa_addr->sa_family != AF_INET)
            continue;

        const auto *interfaceAddress = reinterpret_cast<const struct sockaddr_in *>(entry->ifa_addr);
        if (interfaceAddress->sin_addr.s_addr != source.sin_addr.s_addr)
            continue;

        const auto *interfaceMask = reinterpret_cast<const struct sockaddr_in *>(entry->ifa_netmask);
        setAddress(netmask, interfaceMask->sin_addr.s_addr);
        foundNetmask = true;
        break;
    }
    freeifaddrs(interfaces);

    if (!foundNetmask)
    {
        setError(error, errorCapacity, "could not find the peer-facing interface netmask");
        return false;
    }

    if (error != nullptr && errorCapacity > 0)
        error[0] = '\0';
    return true;
}
