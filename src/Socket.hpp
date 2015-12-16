#ifndef SOCKET_HPP_
#define SOCKET_HPP_

#include "NetCommon.h"
#include <queue>

#define SOCKET_ERROR_CHECK 0

#ifdef _WIN32
typedef SOCKET sock_t;
#else
typedef unsigned sock_t;
#endif

namespace Network {
  // TODO: Use machine_t for socket init. Will help with AI_PASSIVE + bind + getaddrinfo issue.
  typedef enum {CLIENT, HOST} machine_t;
  
  sockaddr_in* CreateAddress(const char * ip, const short port);
  
  class Socket {
  public:
    typedef enum {GOOD = 0, BAD} state_t;
    
    ~Socket(void);
    
    int Bind(sockaddr_in* addr);
    void Close(bool now = true);
    void SetBlocking(bool shouldBlock);
    state_t& GetState(void);
    
  protected:
    Socket(const char * ip, const char * port, int proto, bool shouldBlock = true);

    bool closed, block;
    sock_t s;
    state_t state;
    addr_t addr;
  };

  class TCPSocket : public Socket {
  public:
    TCPSocket(const char * ip, const char * port, bool shouldBlock = true, bool autoConnect = true, unsigned retries = 0, int wait_sec = 2, int wait_usec = 0);
    ~TCPSocket(void);

    int Connect(void);
    int Connect(sockaddr_in* remoteAddr);
    int Listen(int backLog);
    sock_t Accept(sockaddr_in* incomAddr);
    // Overload send and recv to use either our socket, or an accept's socket.
    int Send(sock_t acceptSock, const char* buf, unsigned len);
    int Send(const char* buf, unsigned len);
    int Recv(sock_t acceptSock, char* buf, unsigned len, unsigned off = 0);
    int Recv(char* buf, unsigned len, unsigned off = 0);

    int Update(void);

    bool CanRead(void);
    bool CanSend(void);
  private:
    int AutoConnect(unsigned retries = 0);
    
    timeval tv;
    fd_set readfd, writefd;
    unsigned fdmax;
    bool can_recv, can_send;
  };

  class UDPSocket : public Socket {
  public:
    UDPSocket(const char * port, const char * ip = NULL, bool shouldBlock = true);
    ~UDPSocket(void);
    
    int Send(const char* buf, unsigned len, sockaddr_in* to);
    int Recv(char* buf, unsigned len, sockaddr** from = NULL);
  };
}

#endif
