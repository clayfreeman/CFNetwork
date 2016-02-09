/**
 * @file Socket.cpp
 * @copyright Copyright 2016 Clay Freeman. All rights reserved
 * @license   This project is released under the GNU Lesser General Public
 *            License v3 (LGPL-3.0)
 *
 * Implementation source for the Socket object
 */

#include <arpa/inet.h>     // for inet_ntop
#include <memory>          // for shared_ptr
#include <netinet/in.h>    // for INET_ADDRSTRLEN, INET6_ADDRSTRLEN, sockadd...
#include <string>          // for allocator, operator+, basic_string
#include <sys/errno.h>     // for EBADF, errno
#include <sys/fcntl.h>     // for fcntl, F_GETFD
#include <sys/socket.h>    // for sockaddr_storage, AF_INET, AF_INET6, accep...
#include <unistd.h>        // for close
#include "CFNetwork.hpp"   // for SocketFamily, UnexpectedError, SocketFamil...
#include "Connection.hpp"  // for Connection
#include "Socket.hpp"      // for Socket

namespace CFNetwork {
  /**
   * @brief Socket Constructor
   *
   * Constructs a Socket object given a listening address/port
   *
   * @param addr std::string object containing the listen address
   * @param port int containing the port number to listen on
   */
  Socket::Socket(const std::string& addr, int port) {
    // Ensure the validity of the provided port
    if (port < 1 || port > 65535)
      throw InvalidArgument{"Port number out of range."};
    // Fetch a finalized sockaddr_storage for the given address
    struct sockaddr_storage address  = parseAddress(addr);
    // Create pointers to each expected variety of address family
    struct sockaddr*        address_ =
      reinterpret_cast<struct sockaddr    *>(&address);
    struct sockaddr_in*     address4 =
      reinterpret_cast<struct sockaddr_in *>(&address);
    struct sockaddr_in6*    address6 =
      reinterpret_cast<struct sockaddr_in6*>(&address);
    // Use the appropriate setup helper depending on the address family
    if (address.ss_family == AF_INET) {
      this->family = SocketFamily::IPv4;
      // Store a text-based representation of the listen address
      char addressString[INET_ADDRSTRLEN  + 1] = {};
      this->host = inet_ntop(address.ss_family, &address4->sin_addr,
        addressString, INET_ADDRSTRLEN);
      // Assign the port to the sockaddr struct
      address4->sin_port = htons(this->port = port);
    }
    else if (address.ss_family == AF_INET6) {
      this->family = SocketFamily::IPv6;
      // Store a text-based representation of the listen address
      char addressString[INET6_ADDRSTRLEN + 1] = {};
      this->host = inet_ntop(address.ss_family, &address6->sin6_addr,
        addressString, INET6_ADDRSTRLEN);
      // Assign the port to the sockaddr struct
      address6->sin6_port = htons(this->port = port);
    }
    // Setup the socket using the appropriate address family and type
    this->socket = ::socket(address.ss_family, SOCK_STREAM, 0);
    { // Allow reusing the socket
      int reuse = 1;
      setsockopt(this->socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)); }
    // Attempt to bind the socket to the listening address
    if (bind(this->socket, address_, this->family == SocketFamily::IPv4 ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) < 0) {
      // A problem occurred, close the socket and throw an exception
      close(this->socket);
      throw UnexpectedError{"Couldn't bind to [" + this->host + "]:" +
        std::to_string(this->port)};
    }
    else {
      // Listen with a backlog of 16 clients
      listen(this->socket, 16);
    }
  }

  /**
   * @brief Socket Destructor
   *
   * Upon destruction of a Socket object, close its associated file descriptor
   */
  Socket::~Socket() {
    if (this->valid())
      close(this->socket);
  }

  /**
   * @brief Accept
   *
   * Accepts an incoming client and creates a Connection object for it
   *
   * @remarks This method blocks execution until a client is accepted
   *
   * @return A Connection object for the accepted client
   */
  std::shared_ptr<Connection> Socket::accept() const {
    // Create storage to accept and capture the client's remote address
    std::string raddress{};
    struct sockaddr_storage cli_addr = {};
    socklen_t cli_addr_len = sizeof(struct sockaddr);
    int cli_fd = -1;

    // Check if the Socket is valid
    if (this->valid()) {
      // Accept an incoming client
      cli_fd = ::accept(this->socket, (struct sockaddr*)&cli_addr,
        &cli_addr_len);
      // Store a text-based representation of the remote address
      if (cli_addr.ss_family == AF_INET) {
        // Create a pointer to the appropriate type of structure
        struct sockaddr_in* address =
          reinterpret_cast<struct sockaddr_in *>(&cli_addr);
        // Assign the resulting address to the raddress string
        char addressString[INET_ADDRSTRLEN + 1] = {};
        raddress = inet_ntop(cli_addr.ss_family, &address->sin_addr,
          addressString, INET_ADDRSTRLEN);
      }
      else if (cli_addr.ss_family == AF_INET6) {
        // Create a pointer to the appropriate type of structure
        struct sockaddr_in6* address =
          reinterpret_cast<struct sockaddr_in6*>(&cli_addr);
        // Assign the resulting address to the raddress string
        char addressString[INET6_ADDRSTRLEN + 1] = {};
        raddress = inet_ntop(cli_addr.ss_family, &address->sin6_addr,
          addressString, INET6_ADDRSTRLEN);
      }
      // If cli_fd is negative, an error occurred
      if (cli_fd < 0) {
        throw UnexpectedError{"Couldn't accept client on [" + this->host +
          "]:" + std::to_string(this->port) + " - Invalid client file "
          "descriptor"};
      }
    }
    else {
      // Cannot accept a client on an invalid Socket
      throw UnexpectedError{"Couldn't accept client on [" + this->host +
        "]:" + std::to_string(this->port) + " - Invalid socket"};
    }
    // Return a shared_ptr to the newly created Connection object
    return std::shared_ptr<Connection>{
      new Connection{this->host, raddress, this->port, cli_fd}
    };
  }

  /**
   * @brief Get Family
   *
   * Fetches the address family of the associated Socket
   *
   * @return A SocketFamily value describing the address family
   */
  SocketFamily Socket::getFamily() const {
    return this->family;
  }

  /**
   * @brief Get Host
   *
   * Fetches the listening address of the associated Socket
   *
   * @remarks This method can produce a std::string of either an IPv4 address
   * or an IPv6 address. This method will not produce hostnames
   *
   * @return std::string of the listening address
   */
  const std::string& Socket::getHost() const {
    return this->host;
  }

  /**
   * @brief Get Port
   *
   * Fetches the listening port of the associated Socket
   *
   * @return int of the listening port
   */
  int Socket::getPort() const {
    return this->port;
  }

  /**
   * @brief Valid
   *
   * Determines if the associated Socket has a valid file descriptor
   *
   * @return true if valid, false otherwise
   */
  bool Socket::valid() const {
    return (fcntl(this->socket, F_GETFD) != -1 || errno != EBADF);
  }
}
