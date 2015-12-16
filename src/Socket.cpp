#include "Socket.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#if SOCKET_ERROR_CHECK == 1 && PRINT_DBG
int CheckError(int val) {
  int err =  WSAGetLastError();
  
  if (err != 0) {
    LPVOID buf;
    
    // Thanks microsoft, love this Function...
    FormatMessage(
		  FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, 
		  NULL,
		  err,
		  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		  (LPTSTR) &buf,
		  0, NULL
		  );
    
    printf("Socket: Error %d: %s\n", err, (LPTSTR)buf);
    
    LocalFree(buf);
  }
  
  return err;
}
#else
int CheckError(int val) {
  return WSAGetLastError();
}
#endif
#else
#include <errno.h>

#if SOCKET_ERROR_CHECK == 1 && PRINT_DBG
int CheckError(int val) {
  D_PRINT("Check Error Called.");

  int err = errno;
  
  if (val < 0)
    printf("Socket: Error: %s\n\tCode: %d\n", gai_strerror(err), err);

  return err;
}
#else
int CheckError(int val) {
  return errno;
}
#endif
#endif

namespace Network {
  sockaddr_in* CreateAddress(const char * ip, const short port) {
    D_PRINT("Network: Created new address!");

    sockaddr_in* res = (sockaddr_in*) calloc(sizeof(sockaddr_in), 1);

    res->sin_port = htons(port);
    res->sin_family = AF_INET;

    if (ip == NULL)
#ifdef _WIN32
      res->sin_addr.S_un.S_addr = INADDR_ANY;
#else
    res->sin_addr.s_addr = inet_addr("127.0.0.1");
#endif
    else
      inet_pton(res->sin_family, ip, &(res->sin_addr));

    return res;
  }
  
  Socket::Socket(const char * ip, const char * port, int proto, bool shouldBlock)
    : block(shouldBlock), state(BAD) {
    D_PRINT("Socket: Constructor Called.");

    memset(&addr.hints, 0, sizeof(struct addrinfo));

    addr.hints.ai_family = AF_INET;
    addr.hints.ai_socktype = proto == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM;
    // TODO: If we're binding, we need ai_hints AI_PASSIVE
    addr.hints.ai_flags = 0;

    getaddrinfo(ip == NULL ? "127.0.0.1" : ip, port, &addr.hints, &addr.res);
    
    s = socket(addr.res->ai_family, addr.res->ai_socktype, addr.res->ai_protocol);

    /*
    if (s < 0)
      printf("Socket: Could not create new socket!");
    */
  }
  
  void Socket::SetBlocking(bool shouldBlock) {
    block = shouldBlock;
    
#ifdef _WIN32
    DWORD block_word = block ? 1 : 0;
    
    if (ioctlsocket(s, FIONBIO, &block_word) != 0)
#else
    int options = fcntl(s, F_GETFL, 0);

    if (block) {
      options &= ~O_NONBLOCK;
      D_PRINT("Socket: Set to blocking!");
    }
    else {
      options |= O_NONBLOCK;
      D_PRINT("Socket: Set to non-blocking!");
    }
    
    if (fcntl(s, F_SETFL, options) < 0)
#endif
    {
      printf("Socket: failed to set blocking status on socket.\n");
      Close();
    }
  }
  
  int Socket::Bind(sockaddr_in* addr) {
    D_PRINT("Socket: Bind Called.");
    int sz = sizeof(sockaddr_in);
    int res = bind(s, (sockaddr*) addr, sz);
    int err = CheckError(res);
    
    if (err < 0) {
      state = BAD;
      return ERR;
    }
    
    return res;
  }
  
  void Socket::Close(bool now) {
    D_PRINT("Socket: Close Called.");
    state = BAD;
    if (now) {
#ifdef _WIN32
      closesocket(s);
#else
      close(s);
#endif
      closed = true;
    }
    else
      shutdown(s, SD_BOTH);
  }

  Socket::~Socket() {
    if (!closed) 
      Close(true);

    freeaddrinfo(addr.res);

    D_PRINT("Socket: Destroyed.");
  }

  Socket::state_t& Socket::GetState(void) { return state; }
  
  /****** TCP Socket Implementation ******/
  TCPSocket::TCPSocket(const char * ip, const char * port, bool shouldBlock, bool autoConnect, unsigned retries, int wait_sec, int wait_usec)
    : Socket(ip, port, IPPROTO_TCP, shouldBlock), fdmax(s), can_recv(false), can_send(false) {

    tv.tv_sec = wait_sec;
    tv.tv_usec = wait_usec;
    
    SetBlocking(shouldBlock);
    if (autoConnect) {
      D_PRINT("Socket: Connecting to Peer...\n");

      AutoConnect(retries);
    }

    state = GOOD;

    FD_ZERO(&readfd);
    FD_ZERO(&writefd);
    FD_SET(s, &readfd);
    FD_SET(s, &writefd);
  }
  
  TCPSocket::~TCPSocket(void) {}
  
  int TCPSocket::AutoConnect(unsigned retries) {
    int res;

    do {
      do {
	res = Connect();
      } while (res == EALREADY || res == EINPROGRESS);
      
      if (res != EISCONN && res != 0) {
	D_PRINT("Socket: Could not connect!\n");
	
	if (retries > 0)
	  D_PRINT("Socket: Retrying connection.\n");
      }
      else {
	D_PRINT("Socket: Connected to Peer!\n");
	break;
      }
    } while (retries-- > 0);
    
    return res;
  }

  int TCPSocket::Connect(void) {
    D_PRINT("Socket: Connect Called.");
    int res = connect(s, addr.res->ai_addr, addr.res->ai_addrlen);
    int err = CheckError(res);

    return err;
  }
  
  int TCPSocket::Connect(sockaddr_in* remoteAddr) {
    D_PRINT("Socket: Connect Called.");
    int res = connect(s, (sockaddr*)remoteAddr, sizeof(remoteAddr));
    int err = CheckError(res);

    return err;
  }

  int TCPSocket::Listen(int backLog) {
    D_PRINT("Socket: Listen Called.");
    int max = backLog < 1 ? backLog : SOMAXCONN;
    int res = listen(s, max);
    int err = CheckError(res);
    
    if (err < 0) {
      state = BAD;
      D_PRINT("Socket: Couldn't Listen!\n");
    }
  
    return res;
  }

  sock_t TCPSocket::Accept(sockaddr_in* incomAddr) {
    D_PRINT("Socket: Accept Called.");
    socklen_t sz = sizeof(sockaddr_in);
    sock_t res = accept(s, incomAddr ? (sockaddr *)incomAddr : NULL, incomAddr ? &sz : NULL);
    int err = CheckError((unsigned)res);
    
    if ((err < 0 || res == ENETDOWN) && res != EWOULDBLOCK) {
      state = BAD;
      return ERR;
    }
    
    return res;
  }

  int TCPSocket::Send(sock_t acceptSock, const char* buf, unsigned len) {
    //D_PRINT("Socket: Send Called.");
    int res = send(acceptSock, buf, len, 0);
    int err = CheckError(res);

    //printf("Socket: send = %d\n", res);
    
    if (err > 0 && err != EISCONN)
      return ERR;
    
    return res;
  }

  int TCPSocket::Send(const char* buf, unsigned len) {
    if (can_send) {
      int res = send(s, buf, len, 0);
      int err = CheckError(res);

      //FD_SET(s, &writefd);
      can_send = false;
      
      if (err > 0 && err != EISCONN) {
	//printf("Socket: Error on Send => Err %d\n", err);
	return err;
      }

      //printf("Socket: Sent %d bytes.\n", res);
      
      return res;
    }

    return 0;
  }

  int TCPSocket::Recv(sock_t acceptSock, char* buf, unsigned len, unsigned off) {
    D_PRINT("Socket: Recv Called.");
    int res = recv(acceptSock, buf + off, len, 0);
    int err = CheckError(res);

    if (err < 0)
      buf[0] = '\0';
    else if (err > 0)
      buf[res + off] = '\0';

    return res;
  }

  int TCPSocket::Recv(char* buf, unsigned len, unsigned off) {
    //D_PRINT("Socket: Recv Called.");
    if (can_recv) {
      int res = recv(s, buf + off, len, 0);
      int err = CheckError(res);

      //FD_SET(s, &readfd);
      can_recv = false;
      
      if (res == 0) {
	state = BAD;
	D_PRINT("Socket: Peer closed connection.");
	return ERR;
      }
      else if (res < 0) {
	if (!block && (err == EWOULDBLOCK || err != EAGAIN)) {
	  //printf("Socket: Recv => WOULDBLOCK\n");
	  return 0;
	} else {
	  //printf("Socket: Recv => Unrecoverable Error => %d\n", err);
	  return ERR;
	}
      }
      else {
	// Oh, we actually received something..
	//printf("Socket: Recv %d bytes.\n", res);
	buf[res + off] = '\0';
      }
      
      return res;
    }

    return 0;
  }

  int TCPSocket::Update(void) {
    FD_SET(s, &readfd);
    FD_SET(s, &writefd);
    if (select(fdmax+1, &readfd, &writefd, NULL, &tv) == -1) {
      D_PRINT("Socket: Error in Select!");
      return ERR;
    }
    
    if (FD_ISSET(s, &readfd)) {
      D_PRINT("Socket: Read FD Set!\n");
      can_recv = true;
    } else
      can_recv = false;
    
    if (FD_ISSET(s, &writefd)) {
      can_send = true;
    } else
      can_send = false;

    return 0;
  }

  bool TCPSocket::CanRead(void) { return can_recv; }
  bool TCPSocket::CanSend(void) { return can_send; }
  
  /****** UDP Socket Implementation ******/
  UDPSocket::UDPSocket(const char * port, const char* ip, bool shouldBlock)
    : Socket(ip, port, IPPROTO_UDP, shouldBlock) {
    state = Socket::GOOD;
  }

  UDPSocket::~UDPSocket(void) {
  }
  
  int UDPSocket::Send(const char* buf, unsigned len, sockaddr_in* to) {
    int res = sendto(s, buf, len, 0, (sockaddr *) to, sizeof(*to));
    int err = CheckError(res);
    
    if (err < 0)
      return ERR;
    
    return res;
  }

  int UDPSocket::Recv(char* buf, unsigned len, sockaddr** from) {
    D_PRINT("Socket: RecvFrom called.");
    
    socklen_t addr_sz = sizeof(sockaddr_in);
    int res = recvfrom(s, buf, len, 0, *from, &addr_sz);
    
    int err = CheckError(res);

    if (res <= 0 || err != 0) {
      buf[0] = '\0';
      return ERR;
    }
    else
      buf[res] = '\0'; // Ternary here prevents accidental overrun.

    return res;
  }
}
