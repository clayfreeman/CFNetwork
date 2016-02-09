/**
 * @file Socket.hpp
 * @copyright Copyright 2016 Clay Freeman. All rights reserved
 * @license   This project is released under the GNU Lesser General Public
 *            License v3 (LGPL-3.0)
 *
 * Implementation reference for the Socket object
 */

#ifndef _SOCKET_H
#define _SOCKET_H

#include <memory>         // for shared_ptr
#include <string>         // for string
#include "CFNetwork.hpp"  // for SocketFamily

namespace CFNetwork {
  /**
   * @class Socket
   * @brief An object-oriented encapsulation for sockets
   *
   * The Socket object is responsible for preparations in order to ultimately
   * accept connections on a given listening address and port number
   *
   * @remarks The Socket object is not copyable or assignable since it contains
   * resources that do not lend themselves well to duplication
   */
  class Socket {
    private:
      Socket(const Socket&);
      Socket& operator= (const Socket&);

    protected:
      SocketFamily family = SocketFamily::IPv4;
      std::string  host   = "0.0.0.0";
      int          port   =  0;
      int          socket = -1;

    public:
      Socket(const std::string& addr, int port);
     ~Socket();
      std::shared_ptr<Connection> accept()    const;
      SocketFamily                getFamily() const;
      const std::string&          getHost()   const;
      int                         getPort()   const;
      bool                        valid()     const;
  };
}

#endif
