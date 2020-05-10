// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __COMMON_IP_UTIL_H__
#define __COMMON_IP_UTIL_H__

#include <string>
#include <vector>
#include <uv.h>

inline
bool parse_addr(const char* addr, bool port_canbe_zero,
    std::string &ip, int &port, sockaddr_storage &ss)
{
    const char* sep = strrchr(addr, ':');
    if (sep <= addr)
        return false;
    ip = std::string(addr, sep - addr);

    char* end = nullptr;
    long l = strtol(sep + 1, &end, 10);
    long min_port = port_canbe_zero ? 0 : 1;
    if (l < min_port || l >= 65536 || *end != '\0')
        return false;
    port = (int)l;

    if (uv_ip4_addr(ip.c_str(), port, (sockaddr_in*)&ss) != 0 &&
        uv_ip6_addr(ip.c_str(), port, (sockaddr_in6*)&ss) != 0)
    {
        return false;
    }

    return true;
}

inline
const std::vector<std::string>& resolve_ipany(bool v4)
{
    static std::vector<std::string> ip4s, ip6s;
    static bool inited = false;

    if (!inited)
    {
        int n = 0;
        uv_interface_address_t* ifs = nullptr;
        uv_interface_addresses(&ifs, &n);

        for (int i = 0; i < n; ++i)
        {
            char ip[64] = {0};
            if (ifs[i].address.address4.sin_family == AF_INET)
            {
                uv_ip4_name(&ifs[i].address.address4, ip, sizeof(ip));
                ip4s.push_back(ip);
            }
            else if (ifs[i].address.address6.sin6_family == AF_INET6)
            {
                uv_ip6_name(&ifs[i].address.address6, ip, sizeof(ip));
                ip6s.push_back(ip);
            }
        }

        uv_free_interface_addresses(ifs, n);
        inited = true;
    }

    if (v4)
        return ip4s;
    return ip6s;
}

#endif // __COMMON_IP_UTIL_H__
