/**
 * @file CFNetwork.hpp
 * @copyright Copyright 2016 Clay Freeman.  All rights reserved
 * @license   This project is released under the GNU Lesser General Public
 *            License v3 (LGPL-3.0)
 *
 * Forward declaration of the CFNetwork namespace and related items
 */

#ifndef _CFNETWORK_H
#define _CFNETWORK_H

#include <stdexcept>     // for runtime_error
#include <string>        // for string
#include <sys/socket.h>  // for AF_INET, AF_INET6, sockaddr_storage

/**
 * @namespace CFNetwork
 * @brief Tools to aid in network application development
 *
 * CFNetwork is a collection of objects that simplifies the process of
 * developing an application that will make use of the network
 */
namespace CFNetwork {
  // Provide forward declaration of classes provided by this namespace
  class Connection;
  class Socket;

  // Provide forward declaration of helper functions provided by this namespace
  struct sockaddr_storage parseAddress(const std::string& addr);

  /**
   * @class InvalidArgument
   * @brief Used to signify when an invalid argument blocks action completion
   *
   * The InvalidArgument exception can be thrown by methods in the CFNetwork
   * namespace when an invalid argument is provided. This is a non-critical
   * exception, and can safely be catched.
   */
  class InvalidArgument : public std::runtime_error {
    using std::runtime_error::runtime_error; };

  /**
   * @class UnexpectedError
   * @brief Used to signify when an unexpected error is encountered
   *
   * The UnexpectedError exception can be thrown by methods in the CFNetwork
   * namespace when an unexpected error is encountered. This is a non-critical
   * exception, and can safely be catched
   */
  class UnexpectedError : public std::runtime_error {
    using std::runtime_error::runtime_error; };

  /**
   * @var   MAX_BYTES
   * @brief Maximum number of bytes in a buffer
   *
   * Sets the size of the buffers used by all objects.
   */
  const int MAX_BYTES = 8192;

  /**
   * @enum  ConnectionFlow
   * @brief Used to specify the direction of a Connection
   *
   * The ConnectionFlow enum is responsible for communicating whether or not a
   * given Connection is setup for outbound connectivity or was received inbound
   * from a CFNetwork::Socket object
   */
  enum class ConnectionFlow {
    Inbound,
    Outbound
  };

  /**
   * @enum  SocketFamily
   * @brief Used to specify the family of a Socket
   *
   * The SocketFamily enum is responsible for communicating which address family
   * that a Socket object is using
   */
  enum class SocketFamily {
    IPv4 = AF_INET,
    IPv6 = AF_INET6
  };
}

#endif
