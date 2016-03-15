/**
 * @file Connection.hpp
 * @copyright Copyright 2016 Clay Freeman. All rights reserved
 * @license   This project is released under the GNU Lesser General Public
 *            License v3 (LGPL-3.0)
 *
 * Implementation reference for the Connection object
 */

#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <string>         // for string
#include "CFNetwork.hpp"  // for ConnectionFlow, SocketFamily

namespace CFNetwork {
  /**
   * @class Connection
   * @brief An object-oriented encapsulation for network connections
   *
   * The Connection object is responsible for communication between two network
   * endpoints. The object can be setup by accepting an incoming connection on a
   * CFNetwork::Socket object, or by explicitly connecting to a given address
   * and port combination
   *
   * @remarks The Connection object is not copyable or assignable since it
   * contains resources that do not lend themselves well to duplication
   */
  class Connection {
    private:
      Connection(const Connection&);
      Connection& operator= (const Connection&);

    protected:
      SocketFamily   family = SocketFamily::IPv4;
      ConnectionFlow flow   = ConnectionFlow::Inbound;
      std::string    listen = "";
      int            port   = 0;
      std::string    remote = "0.0.0.0";
      int            socket = -1;

    public:
      Connection(const std::string& addr, int port);
      Connection(const std::string& laddr, const std::string& raddr,
        int port, int socket);
     ~Connection();
      int                getDescriptor()                const;
      SocketFamily       getFamily()                    const;
      ConnectionFlow     getFlow()                      const;
      const std::string& getListen()                    const;
      int                getPort()                      const;
      const std::string& getRemote()                    const;
      std::string        read()                         const;
      bool               valid()                        const;
      void write(std::string data, bool newline = true) const;
  };
}

#endif
