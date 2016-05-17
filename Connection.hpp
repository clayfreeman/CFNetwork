/**
 * @file      Connection.hpp
 * @copyright Copyright 2016 Clay Freeman. All rights reserved
 * @license   GNU Lesser General Public License v3 (LGPL-3.0)
 *
 * Implementation reference for the `Connection` object.
 */

#ifndef _CFNETWORKCONNECTION_H
#define _CFNETWORKCONNECTION_H

#include <string>         // for string
#include "CFNetwork.hpp"  // for ConnectionFlow, SocketFamily

namespace CFNetwork {
  /**
   * @class Connection
   * An object-oriented encapsulation for network connections.
   *
   * The `Connection` object is responsible for communication between two
   * network endpoints. The object can be setup by accepting an incoming
   * connection on a `Socket` object, or by explicitly making an
   * outgoing connection to a given address and port.
   *
   * The `Connection` object is not copyable or assignable since it contains
   * resources that do not lend themselves well to duplication.
   */
  class Connection {
    private:
      Connection(const Connection&);
      Connection& operator= (const Connection&);

    protected:
      /**
       * @var buffer
       * Used to hold intermediate data from the `read(2)` system call to allow
       * for reading up to a specified delimiter.
       */
      std::string    buffer = "";
      /**
       * @var family
       * Used to describe the socket family type of a `Connection`.
       */
      SocketFamily   family = SocketFamily::IPv4;
      /**
       * @var flow
       * Used to describe the connection flow direction of a `Connection`.
       */
      ConnectionFlow flow   = ConnectionFlow::Inbound;
      /**
       * @var listen
       * Holds the listening address associated with an inbound `Connection`.
       */
      std::string    listen = "";
      /**
       * @var port
       * Holds the listening port for an inbound `Connection` or the outbound
       * port for an outbound `Connection`.
       */
      int            port   = 0;
      /**
       * @var remote
       * Holds the remote address of a `Connection`.
       */
      std::string    remote = "0.0.0.0";
      /**
       * @var socket
       * Holds the file descriptor associated with a `Connection`.
       */
      int            socket = -1;

    public:
      Connection(const std::string& addr, int port);
      Connection(const std::string& laddr, const std::string& raddr,
        int port, int socket);
     ~Connection();
      size_t             enqueueData(bool reliable = false, size_t
                           request_length = MAX_BYTES);
      int                getDescriptor()                const;
      SocketFamily       getFamily()                    const;
      ConnectionFlow     getFlow()                      const;
      const std::string& getListen()                    const;
      int                getPort()                      const;
      const std::string& getRemote()                    const;
      std::string        read(bool reliable = false, size_t
                           request_length = MAX_BYTES);
      std::string        readDelim(char delim = '\n');
      bool               valid()                        const;
      void write(std::string data, bool newline = true) const;
  };
}

#endif
