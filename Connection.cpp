/**
 * @file      Connection.cpp
 * @copyright Copyright 2016 Clay Freeman. All rights reserved
 * @license   GNU Lesser General Public License v3 (LGPL-3.0)
 *
 * Implementation source for the `Connection` object.
 */

#include <arpa/inet.h>     // for inet_ntop
#include <cstring>         // for memset
#include <netinet/in.h>    // for INET_ADDRSTRLEN, INET6_ADDRSTRLEN, sockadd...
#include <string>          // for allocator, basic_string, operator+, to_string
#include <sys/errno.h>     // for EBADF, errno
#include <sys/fcntl.h>     // for fcntl, F_GETFD
#include <sys/socket.h>    // for sockaddr_storage, AF_INET, AF_INET6
#include <unistd.h>        // for close, read, write, ssize_t
#include "CFNetwork.hpp"   // for InvalidArgument, parseAddress, SocketFamily
#include "Connection.hpp"  // for Connection

namespace CFNetwork {
  /**
   * `Connection` Constructor (outbound).
   *
   * Allows for constructing a `Connection` object to an outbound endpoint.
   *
   * @param addr The address of the remote endpoint
   * @param port The port of the remote endpoint
   */
  Connection::Connection(const std::string& addr, int port) {
    // Set the ConnectionFlow type to Outbound
    this->flow = ConnectionFlow::Outbound;
    // Ensure the validity of the provided port
    if (port < 1 || port > 65535)
      throw InvalidArgument{"The provided port number is out of range."};
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
      // Store a text-based representation of the remote address
      char addressString[INET_ADDRSTRLEN  + 1] = {};
      this->remote = inet_ntop(address.ss_family, &address4->sin_addr,
        addressString, INET_ADDRSTRLEN);
      // Assign the port to the sockaddr struct
      address4->sin_port = htons(this->port = port);
    }
    else if (address.ss_family == AF_INET6) {
      this->family = SocketFamily::IPv6;
      // Store a text-based representation of the remote address
      char addressString[INET6_ADDRSTRLEN + 1] = {};
      this->remote = inet_ntop(address.ss_family, &address6->sin6_addr,
        addressString, INET6_ADDRSTRLEN);
      // Assign the port to the sockaddr struct
      address6->sin6_port = htons(this->port = port);
    }
    else {
      // Remote address has an unexpected address family
      throw InvalidArgument{"The remote address has an unexpected address "
        "family."};
    }
    // Setup the socket using the appropriate address family and type
    this->socket = ::socket(address.ss_family, SOCK_STREAM, 0);
    { // Allow reusing the socket
      int reuse = 1;
      setsockopt(this->socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)); }
    // Attempt to connect the socket to the remote address
    if (connect(this->socket, address_, this->family == SocketFamily::IPv4 ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) < 0) {
      // A problem occurred, close the socket and throw an exception
      close(this->socket);
      throw UnexpectedError{"Couldn't connect to [" + this->remote + "]:" +
        std::to_string(this->port)};
    }
  }

  /**
   * `Connection` Constructor (inbound).
   *
   * Allows for constructing a `Connection` object from an inbound client file
   * descriptor that was accepted by a listening socket.
   *
   * @param laddr  The address of the local listening socket
   * @param raddr  The address of the remote client
   * @param port   The port of the listening socket that received the client
   * @param socket The file descriptor for the client
   */
  Connection::Connection(const std::string& laddr, const std::string& raddr,
      int port, int socket) {
    // Set the ConnectionFlow type to Inbound
    this->flow = ConnectionFlow::Inbound;
    // Store the provided connection port
    this->port = port;
    // Ensure the validity of the provided port
    if (this->port < 1 || this->port > 65535)
      throw InvalidArgument{"The provided port number is out of range."};
    // Store the provided socket file descriptor for the client
    this->socket = socket;
    // Ensure the validity of the provided socket
    if (!this->valid())
      throw InvalidArgument{"The provided socket file descriptor is invalid."};
    // Verify both addresses and assign them to their instance attributes
    struct sockaddr_storage laddress = parseAddress(laddr);
    struct sockaddr_storage raddress = parseAddress(raddr);
    if (laddress.ss_family == AF_INET && raddress.ss_family == AF_INET) {
      this->family = SocketFamily::IPv4;
      // Create pointers to each expected variety of address family
      struct sockaddr_in* laddress4 =
        reinterpret_cast<struct sockaddr_in*>(&laddress);
      struct sockaddr_in* raddress4 =
        reinterpret_cast<struct sockaddr_in*>(&raddress);
      // Fetch the canonicalized listen address
      char addressString[INET_ADDRSTRLEN  + 1] = {};
      this->listen = inet_ntop(laddress.ss_family, &laddress4->sin_addr,
        addressString, INET_ADDRSTRLEN);
      // Re-zero the addressString buffer for the remote address
      memset(addressString, 0, INET_ADDRSTRLEN);
      // Fetch the canonicalized remote address
      this->remote = inet_ntop(raddress.ss_family, &raddress4->sin_addr,
        addressString, INET_ADDRSTRLEN);
    }
    else if (laddress.ss_family == AF_INET6 && raddress.ss_family == AF_INET6) {
      this->family = SocketFamily::IPv6;
      // Create pointers to each expected variety of address family
      struct sockaddr_in6* laddress6 =
        reinterpret_cast<struct sockaddr_in6*>(&laddress);
      struct sockaddr_in6* raddress6 =
        reinterpret_cast<struct sockaddr_in6*>(&raddress);
      // Fetch the canonicalized listen address
      char addressString[INET6_ADDRSTRLEN  + 1] = {};
      this->listen = inet_ntop(laddress.ss_family, &laddress6->sin6_addr,
        addressString, INET6_ADDRSTRLEN);
      // Re-zero the addressString buffer for the remote address
      memset(addressString, 0, INET6_ADDRSTRLEN);
      // Fetch the canonicalized remote address
      this->remote = inet_ntop(raddress.ss_family, &raddress6->sin6_addr,
        addressString, INET6_ADDRSTRLEN);
    }
    else {
      // Listen/remote addresses shouldn't have differing address families
      throw InvalidArgument{"The listen address and remote address have "
        "differing or unexpected address families."};
    }
  }

  /**
   * `Connection` Destructor.
   *
   * Upon destruction of a `Connection` object, close its associated file
   * descriptor (if still valid).
   */
  Connection::~Connection() {
    if (this->valid())
      close(this->socket);
  }

  /**
   * Fetches the file descriptor of the `Connection` instance.
   *
   * The internal file descriptor can be used to perform more advanced actions
   * that this class doesn't accommodate for.
   *
   * @return `int` representing a file descriptor.
   */
  int Connection::getDescriptor() const {
    return this->socket;
  }

  /**
   * Fetches the address family of the `Connection` instance.
   *
   * @see    SocketFamily for more information on socket families.
   *
   * @return `SocketFamily` value describing the address family.
   */
  SocketFamily Connection::getFamily() const {
    return this->family;
  }

  /**
   * Fetches the flow type of the `Connection` instance.
   *
   * @see    ConnectionFlow for more information on flow types.
   *
   * @return `ConnectionFlow` value describing the flow type.
   */
  ConnectionFlow Connection::getFlow() const {
    return this->flow;
  }

  /**
   * Fetches the listening address of the `Connection` instance.
   *
   * This method will produce a `std::string` of an IPv4/IPv6 address only (no
   * IP addresses will be reverse resolved into hostnames).
   *
   * In the context of an outbound `Connection`, the resulting value will be an
   * empty `std::string`.
   *
   * @return `std::string` containing the listening address.
   */
  const std::string& Connection::getListen() const {
    return this->listen;
  }

  /**
   * Fetches the port of the `Connection` instance.
   *
   * If the `Connection` represents an inbound client, the port will be that of
   * the originating `Socket` listening port. For outbound connections, the port
   * will be the original value provided during construction.
   *
   * @return `int` representing the port.
   */
  int Connection::getPort() const {
    return this->port;
  }

  /**
   * Fetches the remote address of the `Connection` instance.
   *
   * This method will produce a `std::string` of an IPv4/IPv6 address only (no
   * IP addresses will be reverse resolved into hostnames).
   *
   * @return `std::string` containing the remote peer's IP address.
   */
  const std::string& Connection::getRemote() const {
    return this->remote;
  }

  /**
   * Attempts to read data from the internal file descriptor.
   *
   * Performs a blocking read on the internal file descriptor up to
   * `MAX_BYTES - 1`. If there were zero bytes read then the `Connection` will
   * be invalidated due to being reset by the remote peer.
   *
   * @throws `InvalidArgument` if the `Connection` is invalid.
   * @throws `UnexpectedError` if the `Connection` was reset by peer.
   *
   * @return `std::string` containing the data that was read.
   */
  std::string Connection::read() const {
    // Prepare storage for the return value
    std::string data;
    // Make sure the socket is valid (open)
    if (this->valid()) {
      // Prepare a buffer for the incoming data
      char buffer[MAX_BYTES] = {};
      // Read up to (MAX_BYTES - 1) bytes from the file descriptor to ensure a
      // null character at the end to prevent overflow
      ssize_t count = ::read(this->socket, buffer, MAX_BYTES - 1);
      // Copy the C-String into a std::string
      data = buffer;
      // If there was 0 bytes of data to read ...
      if (count < 1) {
        // this->socket marked readable, but no data was read; connection closed
        close(this->socket);
        throw UnexpectedError{"Connection reset by peer " + this->remote +
          ":" + std::to_string(this->port)};
      }
    }
    else {
      // Throw an exception regarding the invalidity of the socket
      throw InvalidArgument{"The socket file descriptor is invalid."};
    }
    // Return a copy of the data that was read
    return data;
  }

  /**
   * Determines if the file descriptor is considered valid for read, write, or
   * any other operations.
   *
   * A file descriptor is considered invalid if a call requesting its flags
   * fails with the return value of `-1` or `errno` is set to `EBADF` (the
   * provided argument is not an open file descriptor). If neither case is
   * satisfied, the file descriptor is considered valid.
   *
   * @see    fcntl() For more information regarding this procedure's test.
   *
   * @return `true` if the file descriptor is valid, `false` otherwise.
   */
  bool Connection::valid() const {
    return (fcntl(this->socket, F_GETFD) != -1 || errno != EBADF);
  }

  /**
   * Attempts to write the provided data to the internal file descriptor.
   *
   * An optional newline character is inserted into the provided data by
   * default, however this can be avoided using the appropriate parameter for
   * this method.
   *
   * @throws `InvalidArgument` if the internal file descriptor is
   *         considered invalid.
   *
   * @param  data    `std::string` containing the contents to write
   * @param  newline Whether or not a newline character should be included
   */
  void Connection::write(std::string data, bool newline) const {
    if (newline) data += "\n";
    if (this->valid()) ::write(this->socket, data.c_str(), data.length());
    else throw InvalidArgument{"The socket file descriptor is invalid."};
  }
}
