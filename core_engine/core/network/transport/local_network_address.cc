#include "core/network/transport/local_network_address.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

namespace CoreEngine {
    namespace {
        void AppendAddress(std::string &output, const char *address) {
            if (address == nullptr || address[0] == '\0') {
                return;
            }

            if (!output.empty()) {
                output.append(", ");
            }
            output.append(address);
        }

#if defined(_WIN32)
        [[nodiscard]] bool IsUsableAdapter(const IP_ADAPTER_ADDRESSES &adapter) noexcept {
            return adapter.OperStatus == IfOperStatusUp &&
                   (adapter.IfType == IF_TYPE_ETHERNET_CSMACD ||
                    adapter.IfType == IF_TYPE_IEEE80211);
        }

        void AppendWindowsIPv4(std::string &output, const SOCKADDR_IN &address) {
            const std::uint32_t ip = ntohl(address.sin_addr.S_un.S_addr);
            const std::uint8_t a = static_cast<std::uint8_t>((ip >> 24u) & 0xffu);
            const std::uint8_t b = static_cast<std::uint8_t>((ip >> 16u) & 0xffu);
            const std::uint8_t c = static_cast<std::uint8_t>((ip >> 8u) & 0xffu);
            const std::uint8_t d = static_cast<std::uint8_t>(ip & 0xffu);
            if (a == 0u || a == 127u || (a == 169u && b == 254u)) {
                return;
            }

            char buffer[16]{};
            std::snprintf(buffer,
                          sizeof(buffer),
                          "%u.%u.%u.%u",
                          static_cast<unsigned int>(a),
                          static_cast<unsigned int>(b),
                          static_cast<unsigned int>(c),
                          static_cast<unsigned int>(d));
            AppendAddress(output, buffer);
        }
#endif
    } // namespace

    std::string QueryLocalNetworkAddressText() {
        std::string output;

#if defined(_WIN32)
        constexpr ULONG kFlags = GAA_FLAG_SKIP_ANYCAST |
                                 GAA_FLAG_SKIP_MULTICAST |
                                 GAA_FLAG_SKIP_DNS_SERVER;
        ULONG buffer_size = 16 * 1024;
        std::vector<std::byte> buffer(buffer_size);

        auto *addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
        ULONG result = GetAdaptersAddresses(AF_INET, kFlags, nullptr, addresses, &buffer_size);
        if (result == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(buffer_size);
            addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
            result = GetAdaptersAddresses(AF_INET, kFlags, nullptr, addresses, &buffer_size);
        }

        if (result == NO_ERROR) {
            for (const IP_ADAPTER_ADDRESSES *adapter = addresses;
                 adapter != nullptr;
                 adapter = adapter->Next) {
                if (!IsUsableAdapter(*adapter)) {
                    continue;
                }

                for (const IP_ADAPTER_UNICAST_ADDRESS *unicast = adapter->FirstUnicastAddress;
                     unicast != nullptr;
                     unicast = unicast->Next) {
                    if (unicast->Address.lpSockaddr == nullptr ||
                        unicast->Address.lpSockaddr->sa_family != AF_INET) {
                        continue;
                    }

                    AppendWindowsIPv4(output, *reinterpret_cast<const SOCKADDR_IN *>(unicast->Address.lpSockaddr));
                }
            }
        }
#elif defined(__APPLE__) || defined(__linux__)
        ifaddrs *interfaces = nullptr;
        if (getifaddrs(&interfaces) == 0) {
            for (const ifaddrs *iface = interfaces; iface != nullptr; iface = iface->ifa_next) {
                if (iface->ifa_addr == nullptr ||
                    iface->ifa_addr->sa_family != AF_INET ||
                    (iface->ifa_flags & IFF_LOOPBACK) != 0 ||
                    (iface->ifa_flags & IFF_UP) == 0) {
                    continue;
                }

                char buffer[INET_ADDRSTRLEN]{};
                const auto *address = reinterpret_cast<const sockaddr_in *>(iface->ifa_addr);
                AppendAddress(output, inet_ntop(AF_INET, &address->sin_addr, buffer, sizeof(buffer)));
            }

            freeifaddrs(interfaces);
        }
#endif

        return output.empty() ? std::string{"unavailable"} : output;
    }
} // namespace CoreEngine
