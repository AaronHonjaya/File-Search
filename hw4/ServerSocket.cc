/*
 * Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include "./ServerSocket.h"

#include <arpa/inet.h>   // for inet_ntop()
#include <errno.h>       // for errno, used by strerror()
#include <netdb.h>       // for getaddrinfo()
#include <stdio.h>       // for snprintf()
#include <string.h>      // for memset, strerror()
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <unistd.h>      // for close(), fcntl()

#include <iostream>  // for std::cerr, etc.

extern "C" {
#include "libhw1/CSE333.h"
}

using namespace std;
namespace hw4 {

static bool ClientGetIPandPort(struct sockaddr* addr, string* const IP_ret_addr,
                               uint16_t* const port_ret_addr);
static bool ServerGetIPandPort(int client_fd, int sock_family,
                               string* const IP_ret_addr,
                               string* const dns_name_ret_addr);

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1) close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int* const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:
  int chosen_ai_family = ai_family == AF_UNSPEC ? AF_INET6 : ai_family;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = chosen_ai_family;
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  struct addrinfo* result;
  std::string portnum = std::to_string(port_);
  int res = getaddrinfo(nullptr, portnum.c_str(), &hints, &result);

  // Did addrinfo() fail?
  if (res != 0) {
    std::cerr << "getaddrinfo() failed: ";
    std::cerr << gai_strerror(res) << std::endl;
    return false;
  }

  int curr_listen_fd = -1;
  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    curr_listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (curr_listen_fd == -1) {
      std::cerr << "socket() failed: " << strerror(errno) << std::endl;
      curr_listen_fd = -1;
      continue;
    }

    int optval = 1;
    setsockopt(curr_listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
               sizeof(optval));

    if (bind(curr_listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      sock_family_ = rp->ai_family;
      break;
    }
    close(curr_listen_fd);
    curr_listen_fd = -1;
  }

  freeaddrinfo(result);

  if (curr_listen_fd == -1) return false;

  if (listen(curr_listen_fd, SOMAXCONN) != 0) {
    std::cerr << "Failed to mark socket as listening: ";
    std::cerr << strerror(errno) << std::endl;
    close(curr_listen_fd);
    return false;
  }

  listen_sock_fd_ = curr_listen_fd;
  *listen_fd = curr_listen_fd;
  return true;
}

bool ServerSocket::Accept(int* const accepted_fd,
                          std::string* const client_addr,
                          uint16_t* const client_port,
                          std::string* const client_dns_name,
                          std::string* const server_addr,
                          std::string* const server_dns_name) const {
  // Accept a new connection on the listening socket listen_sock_fd_.
  // (Block until a new connection arrives.)  Return the newly accepted
  // socket, as well as information about both ends of the new connection,
  // through the various output parameters.

  // STEP 2:
  while (1) {
    struct sockaddr_storage caddr;
    socklen_t caddr_len = sizeof(caddr);
    int client_fd =
        accept(listen_sock_fd_, reinterpret_cast<struct sockaddr*>(&caddr),
               &caddr_len);

    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EINTR) continue;

      cerr << "Failure with accept: " << strerror(errno) << endl;
      return false;
    }
    *accepted_fd = client_fd;
    if (!ClientGetIPandPort(reinterpret_cast<struct sockaddr*>(&caddr),
                            client_addr, client_port)) {
      return false;
    }

    char hostname[1024];
    if (getnameinfo(reinterpret_cast<struct sockaddr*>(&caddr), caddr_len,
                    hostname, 1024, nullptr, 0, 0) != 0) {
      return false;
    }
    *client_dns_name = hostname;

    ServerGetIPandPort(client_fd, sock_family_, server_addr, server_dns_name);
    break;
  }
  return true;
}

static bool ClientGetIPandPort(struct sockaddr* addr, string* const IP_ret_addr,
                               uint16_t* const port_ret_addr) {
  if (addr->sa_family == AF_INET) {
    char ip_addr[INET_ADDRSTRLEN];
    struct sockaddr_in* in4 = reinterpret_cast<struct sockaddr_in*>(addr);
    inet_ntop(AF_INET, &(in4->sin_addr), ip_addr, INET_ADDRSTRLEN);
    *IP_ret_addr = ip_addr;
    *port_ret_addr = ntohs(in4->sin_port);
    return true;

  } else if (addr->sa_family == AF_INET6) {
    char ip_addr[INET6_ADDRSTRLEN];
    struct sockaddr_in6* in6 = reinterpret_cast<struct sockaddr_in6*>(addr);
    inet_ntop(AF_INET6, &(in6->sin6_addr), ip_addr, INET6_ADDRSTRLEN);
    *IP_ret_addr = ip_addr;
    *port_ret_addr = ntohs(in6->sin6_port);
    return true;

  } else {
    cerr << "Invalid address and port" << endl;
    return false;
  }
}

static bool ServerGetIPandPort(int client_fd, int sock_family,
                               string* const IP_ret_addr,
                               string* const dns_name_ret_addr) {
  char hostname[1024];
  hostname[0] = '\0';

  std::cout << "Server side interface is ";
  if (sock_family == AF_INET) {
    // The server is using an IPv4 address.
    struct sockaddr_in srvr;
    socklen_t srvrlen = sizeof(srvr);
    char ip_addr[INET_ADDRSTRLEN];
    getsockname(client_fd, (struct sockaddr*)&srvr, &srvrlen);
    inet_ntop(AF_INET, &srvr.sin_addr, ip_addr, INET_ADDRSTRLEN);
    *IP_ret_addr = ip_addr;
    // Get the server's dns name, or return it's IP address as
    // a substitute if the dns lookup fails.
    getnameinfo((const struct sockaddr*)&srvr, srvrlen, hostname, 1024, nullptr,
                0, 0);
    *dns_name_ret_addr = hostname;
  } else {
    // The server is using an IPv6 address.
    struct sockaddr_in6 srvr;
    socklen_t srvrlen = sizeof(srvr);
    char ip_addr[INET6_ADDRSTRLEN];
    getsockname(client_fd, (struct sockaddr*)&srvr, &srvrlen);
    inet_ntop(AF_INET6, &srvr.sin6_addr, ip_addr, INET6_ADDRSTRLEN);
    *IP_ret_addr = ip_addr;
    // Get the server's dns name, or return it's IP address as
    // a substitute if the dns lookup fails.
    getnameinfo((const struct sockaddr*)&srvr, srvrlen, hostname, 1024, nullptr,
                0, 0);
    *dns_name_ret_addr = hostname;
  }
  return true;
}

}  // namespace hw4
