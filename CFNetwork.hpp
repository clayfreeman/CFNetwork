/**
 * @file      CFNetwork.hpp
 * @copyright Copyright 2016 Clay Freeman. All rights reserved
 * @license   GNU Lesser General Public License v3 (LGPL-3.0)
 *
 * Forward declaration of the `CFNetwork` namespace and related items.
 */

#ifndef _CFNETWORK_H
#define _CFNETWORK_H

#include <stdexcept>     // for runtime_error
#include <string>        // for string
#include <sys/socket.h>  // for AF_INET, AF_INET6, SOCK_DGRAM, SOCK_STREAM, ...

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// Define macros to help select the appropriate address family for things like
// `inet_ntop()` and assigning a port to a `sockaddr` struct
#define addr_(i) reinterpret_cast<struct sockaddr*>(&i)
#define addr4(i) reinterpret_cast<void*>(\
  &reinterpret_cast<struct sockaddr_in *>(&i)->sin_addr)
#define addr6(i) reinterpret_cast<void*>(\
  &reinterpret_cast<struct sockaddr_in6*>(&i)->sin6_addr)
#define port4(i) reinterpret_cast<uint16_t*>(\
  &reinterpret_cast<struct sockaddr_in *>(&i)->sin_port)
#define port6(i) reinterpret_cast<uint16_t*>(\
  &reinterpret_cast<struct sockaddr_in6*>(&i)->sin6_port)
#endif

/**
 * @namespace CFNetwork
 * `CFNetwork` is a collection of utilities that simplifies the process of
 * developing an application that will make use of the network.
 */
namespace CFNetwork {
  // Provide forward declaration of classes provided by this namespace
  class Connection;
  class Socket;

  // Provide forward declaration of helper functions provided by this namespace
  struct sockaddr_storage parseAddress(const std::string& addr);

  /**
   * @class InvalidArgument
   * The `InvalidArgument` exception can be thrown by methods in the `CFNetwork`
   * namespace when an invalid argument is provided.
   *
   * This is a non-critical exception, and can safely be caught.
   */
  class InvalidArgument : public std::runtime_error {
    using std::runtime_error::runtime_error; };

  /**
   * @class UnexpectedError
   * The `UnexpectedError` exception can be thrown by methods in the `CFNetwork`
   * namespace when an unexpected error is encountered.
   *
   * This is a non-critical exception, and can safely be caught.
   */
  class UnexpectedError : public std::runtime_error {
    using std::runtime_error::runtime_error; };

  /**
   * @var MAX_BYTES
   * The maximum number of bytes that should be contained within all buffers in
   * this namespace's classes.
   */
  const int MAX_BYTES = 8192;

  /**
   * @enum ConnectionFlow
   * The `ConnectionFlow` enum is responsible for communicating whether or not a
   * given `Connection` is setup for outbound connectivity or was received
   * inbound from a `Socket` object.
   */
  enum class ConnectionFlow {
    /**
     * @var Inbound
     * Represents an inbound `Connection`.
     */
    Inbound,
    /**
     * @var Outbound
     * Represents an outbound `Connection`.
     */
    Outbound
  };

  /**
   * @enum SocketFamily
   * The `SocketFamily` enum is responsible for communicating which address
   * family that a `Socket` object is using.
   */
  enum class SocketFamily {
    /**
     * @var IPv4
     * Refers to the `AF_INET` socket family.
     */
    IPv4 = AF_INET,
    /**
     * @var IPv6
     * Refers to the `AF_INET6` socket family.
     */
    IPv6 = AF_INET6
  };

  // /*
  //  * @enum SocketType
  //  * The `SocketType` enum is responsible for communicating whether a given
  //  * `Socket` object is using TCP or UDP as its transport.
  //  */
  // enum class SocketType {
  //   TCP = SOCK_STREAM,
  //   UDP = SOCK_DGRAM
  // };
}

#endif
