/**
 * @file      Socket.hpp
 * @copyright Copyright 2016 Clay Freeman. All rights reserved
 * @license   GNU Lesser General Public License v3 (LGPL-3.0)
 *
 * Implementation reference for the `Socket` object.
 */

#ifndef _SOCKET_H
#define _SOCKET_H

#include <memory>         // for shared_ptr
#include <string>         // for string
#include "CFNetwork.hpp"  // for SocketFamily

namespace CFNetwork {
  /**
   * @class Socket
   * An object-oriented encapsulation for sockets.
   *
   * The `Socket` object is responsible for preparations in order to ultimately
   * accept connections on a given listening address and port number.
   *
   * The `Socket` object is not copyable or assignable since it contains
   * resources that do not lend themselves well to duplication.
   */
  class Socket {
    private:
      Socket(const Socket&);
      Socket& operator= (const Socket&);

    protected:
      /**
       * @var family
       * Used to describe the socket family type of a `Socket`.
       */
      SocketFamily family = SocketFamily::IPv4;
      /**
       * @var host
       * Holds the listening address associated with a `Socket`.
       */
      std::string  host   = "0.0.0.0";
      /**
       * @var port
       * Holds the listening port associated with a `Socket`.
       */
      int          port   =  0;
      /**
       * @var socket
       * Holds the file descriptor associated with a `Socket`.
       */
      int          socket = -1;

    public:
      Socket(const std::string& addr, int port);
     ~Socket();
      std::shared_ptr<Connection> accept()        const;
      int                         getDescriptor() const;
      SocketFamily                getFamily()     const;
      const std::string&          getHost()       const;
      int                         getPort()       const;
      bool                        valid()         const;
  };
}

#endif
