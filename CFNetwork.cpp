/**
 * @file CFNetwork.cpp
 * @copyright Copyright 2016 Clay Freeman.  All rights reserved
 * @license   This project is released under the GNU Lesser General Public
 *            License v3 (LGPL-3.0)
 *
 * Implementation source for the CFNetwork helper functions
 */

#include <cstring>        // for memcpy
#include <netdb.h>        // for addrinfo, freeaddrinfo, getaddrinfo
#include <string>         // for string
#include <sys/socket.h>   // for sockaddr_storage, AF_INET, AF_INET6
#include "CFNetwork.hpp"  // for InvalidArgument

namespace CFNetwork {
  /**
   * @brief Parse Address
   *
   * Dynamically parse a std::string into a sockaddr_storage structure capable
   * of being used in socket operations
   *
   * @remarks The `struct sockaddr_storage` can be reinterpret cast into any of
   * the following structures (after checking the ss_family attribute):
   *   - `struct sockaddr`
   *   - `struct sockaddr_in`
   *   - `struct sockaddr_in6`
   *
   * @throws `CFNetwork::InvalidArgument` on failure or when an unexpected
   * address family is encountered
   *
   * @return `struct sockaddr_storage` containing the relevant information
   */
  struct sockaddr_storage parseAddress(const std::string& addr) {
    // Declare storage for the results
    struct sockaddr_storage address = {};
    struct addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags  = AI_NUMERICHOST;

    // Upon failure to parse or unexpected address family, throw an exception
    if (getaddrinfo(addr.c_str(), nullptr, &hints, &res) != 0 ||
        res == nullptr || (res->ai_family != AF_INET &&
        res->ai_family != AF_INET6))
      throw InvalidArgument{"Could not parse the provided address."};

    // Copy the first address into the
    memcpy(&address, res->ai_addr, res->ai_addrlen);
    // Free the required storage for getaddrinfo(...)
    freeaddrinfo(res);

    return address;
  }
}
