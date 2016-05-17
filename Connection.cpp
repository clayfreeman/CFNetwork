/**
 * @file      Connection.cpp
 * @copyright Copyright 2016 Clay Freeman. All rights reserved
 * @license   GNU Lesser General Public License v3 (LGPL-3.0)
 *
 * Implementation source for the `Connection` object.
 */

#include <arpa/inet.h>     // for inet_ntop
#include <cassert>         // for assert
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
  #ifndef DOXYGEN_SHOULD_SKIP_THIS
  const auto& read_fn = ::read;
  #endif

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
    // Use the appropriate setup helper depending on the address family
    if (address.ss_family == AF_INET || address.ss_family == AF_INET6) {
      // Assign the appropriate address family to describe the `Connection`
      this->family = (address.ss_family == AF_INET ?
        SocketFamily::IPv4 : SocketFamily::IPv6);
      // Determine the appropriate pointer type for the remote address
      auto addrPtr = (address.ss_family == AF_INET ?
        addr4(address) : addr6(address));
      // Store a text-based representation of the remote address
      char addressString[INET6_ADDRSTRLEN + 1] = {};
      this->remote = inet_ntop(address.ss_family, addrPtr, addressString,
        INET6_ADDRSTRLEN);
      // Assign the port to the sockaddr struct
      *(this->family == SocketFamily::IPv4 ? port4(address) : port6(address)) =
        htons(this->port = port);
    }
    else {
      // Remote address has an unexpected address family
      throw InvalidArgument{"The remote address has an unexpected address "
        "family."};
    }
    // Setup the socket using the appropriate address family and type
    this->socket = ::socket(address.ss_family, SOCK_STREAM, 0);
    // Attempt to connect the socket to the remote address
    if (connect(this->socket, addr_(address),
        this->family == SocketFamily::IPv4 ?
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
    // Determine if the listening and remote addresses are valid
    if (laddress.ss_family == raddress.ss_family &&
       (laddress.ss_family == AF_INET || laddress.ss_family == AF_INET6)) {
      // Assign the appropriate address family to describe the `Connection`
      this->family  = (laddress.ss_family == AF_INET ?
        SocketFamily::IPv4 : SocketFamily::IPv6);
      // Determine the appropriate pointer type for both addresses
      auto laddrPtr = (laddress.ss_family == AF_INET ?
             addr4(laddress) : addr6(laddress)),
           raddrPtr = (raddress.ss_family == AF_INET ?
             addr4(raddress) : addr6(raddress));
      // Fetch the canonicalized listen address
      char addressString[INET6_ADDRSTRLEN + 1] = {};
      this->listen  = inet_ntop(laddress.ss_family, laddrPtr, addressString,
        INET6_ADDRSTRLEN);
      // Re-zero the addressString buffer for the remote address
      memset(addressString, 0, INET6_ADDRSTRLEN);
      // Fetch the canonicalized remote address
      this->remote  = inet_ntop(raddress.ss_family, raddrPtr, addressString,
        INET6_ADDRSTRLEN);
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
   * Enqueue data from the internal file descriptor to the internal buffer.
   *
   * Performs a blocking read on the internal file descriptor and enqueues the
   * resulting data to the internal buffer. Requests to enqueue data can either
   * be reliable or unreliable as described below:
   *
   * Reliable requests use one or more calls to `read(2)` to accomplish the goal
   * of enqueuing exactly `request_length` bytes to the internal buffer. The
   * return value of this type of request is predictable and should match
   * `request_length`.
   *
   * Unreliable requests use only one call to `read(2)` using the smallest value
   * between `MAX_BYTES` and `request_length`. The return value of this type of
   * request is not predictable and is more likely to differ from
   * `request_length` than a reliable request (even under normal circumstances).
   *
   * Reliable requests provide the advantage that, if possible, the requested
   * number of bytes will be enqueued before returning to the original caller.
   * However, the downside to reliable requests is that there is an indefinite
   * waiting period for data if there is less data available than requested.
   *
   * Unreliable requests ensure that there will be no waiting period if there is
   * data available to read. However, the amount of data that is enqueued is not
   * predictable.
   *
   * Each type of request can fail if the connection is reset by the remote peer
   * and an exception will be thrown.
   *
   * @throws `InvalidArgument` if the `Connection` is invalid or the requested
   *                           length is invalid.
   * @throws `UnexpectedError` if the `Connection` was reset by peer.
   *
   * @param  reliable       Whether or not the request should be reliable (true)
   *                        or unreliable (false)
   * @param  request_length The total number of bytes to enqueue to the internal
   *                        buffer
   *
   * @return                The number of bytes that were enqueued to the
   *                        internal buffer.
   */
  size_t Connection::enqueueData(bool reliable, size_t request_length) {
    assert(MAX_BYTES > 0);
    // Check if the requested length is valid
    if (request_length == 0)
      throw InvalidArgument{"The requested length is invalid."};
    // If a request was made for a reliable read then use the provided
    // `request_length` as the `read_length` target. Otherwise use the minimum
    // value between `request_length` and `MAX_BYTES`
     size_t read_length = request_length = (reliable ? request_length :
       (request_length < MAX_BYTES ? request_length : MAX_BYTES));
    ssize_t return_val  = 0;
    // Check if the file descriptor is valid
    if (!this->valid())
      throw InvalidArgument{"The socket file descriptor is invalid."};
    // Only attempt to enqueue data if a request for more than 0 bytes was made
    if (read_length > 0) do {
      // Prepare a zero-filled buffer to temporarily store the incoming data
      // from the upcoming call to `read(2)`
      unsigned char buffer[MAX_BYTES] = {};
      // Read up to `MAX_BYTES` or `read_length` bytes (whichever is smallest)
      // from the file descriptor
      return_val = read_fn(this->socket, buffer, read_length <= MAX_BYTES ?
        read_length : MAX_BYTES);
      // If the `read(2)` system call was successful (>= 0) then process the
      // data that was temporarily stored in the buffer
      if (return_val >= 0) {
        // Cast the return value to an unsigned `size_t` type to measure the
        // amount of data that was read
        size_t data_read = static_cast<size_t>(return_val);
        // Append the data that was read to the internal buffer using the return
        // value of the `read(2)` system call as the data size
        this->buffer    += std::string{reinterpret_cast<const char*>(buffer),
                                       data_read};
        // Adjust the appropriate counters using the return value of this
        // iteration's call to `read(2)`
        read_length     -= data_read;
      } else {
        // Close the internal file descriptor
        close(this->socket);
        // Throw an exception explaining the error
        throw UnexpectedError{"Connection reset by peer " + this->remote +
          ":" + std::to_string(this->port)};
      }
    // Continue looping if trying to read in a reliable fashion and there is
    // still data to read
    } while (reliable && return_val >= 0 && read_length > 0);
    // Return the total length of data that was enqueued to the internal buffer
    return request_length - read_length;
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
   * @see    `SocketFamily` for more information on socket families.
   *
   * @return `SocketFamily` value describing the address family.
   */
  SocketFamily Connection::getFamily() const {
    return this->family;
  }

  /**
   * Fetches the flow type of the `Connection` instance.
   *
   * @see    `ConnectionFlow` for more information on flow types.
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
   * Attempts to read data from the internal buffer & file descriptor.
   *
   * Requests to read data will check the internal buffer to determine if a
   * sufficient amount of data is available to satisfy the request. If not, an
   * attempt is made to enqueue more data by reading from the file descriptor.
   *
   * Requests to read data can either be reliable or unreliable as described by
   * the `enqueueData()` method of this class.
   *
   * Exceptions can occur from the `enqueueData()` method that will not be
   * caught by this method.
   *
   * @see    `enqueueData()` for more information regarding reliable/unreliable
   *                         requests and potential exceptions.
   *
   * @param  reliable       Whether or not the request should be reliable (true)
   *                        or unreliable (false)
   * @param  request_length The total number of bytes to read
   *
   * @return                The resulting `std::string` of the requested data.
   */
  std::string Connection::read(bool reliable, size_t request_length) {
    assert(MAX_BYTES > 0);
    // Keep track of the length of the internal buffer
    size_t buf_length = this->buffer.length();
    // Determine if some data needs to be enqueued
    if (buf_length < request_length) {
      // Enqueue the necessary amount of data (if there is not enough data
      // available to satisfy the request)
      size_t remaining_length = reliable ?
        request_length - buf_length : MAX_BYTES;
      // Attempt to enqueue the remaining amount of data
      if (remaining_length > 0) this->enqueueData(reliable, remaining_length);
      // Update the value of the buffer length
      buf_length = this->buffer.length();
    }
    // Calculate the maximum bound of the buffer
    size_t str_length = request_length < buf_length ?
      request_length : buf_length;
    // Generate the resulting return value for this read
    std::string data = this->buffer.substr(0, str_length);
    // Adjust the internal buffer to remove the requested data
    this->buffer.erase(0, str_length);
    return data;
  }

  /**
   * Attempts to read a string up to the specified delimiter.
   *
   * Requests to read up to a delimiter will result in a search of the internal
   * buffer to find the specified delimiter. If the delimiter is not found then
   * `enqueueData()` will be called until the delimiter can be found.
   *
   * Once the specified delimiter is found all data up to (and including) the
   * delimiter will be extracted from the buffer and returned to the caller.
   *
   * Exceptions can occur from the `enqueueData()` method that will not be
   * caught by this method.
   *
   * @see    `enqueueData()` for more information regarding how data is enqueued
   *                         to the internal buffer and potential exceptions.
   *
   * @return The resulting `std::string` of the requested data.
   */
  std::string Connection::readDelim(char delim) {
    size_t offset = 0, location;
    // Continue enqueuing data until the specified delimiter is found
    while ((location = this->buffer.find(delim, offset)) == std::string::npos) {
      // Calculate the offset for the next search
      offset  = this->buffer.length();
      // Attempt to enqueue more data for the next search
      this->enqueueData();
    } ++location;
    // Copy the contents of the buffer up to the resulting location
    std::string data = this->buffer.substr(0, location);
    // Erase the extracted portion of the buffer
    this->buffer.erase(0, location);
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
   * @see    `fcntl(2)` for more information regarding this procedure's test.
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
