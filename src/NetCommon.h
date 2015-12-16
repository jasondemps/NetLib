#ifndef NETCOMMON_H_
#define NETCOMMON_H_

#define PRINT_DBG 0
#define ERR -1

#define UDP_MAX 63999

#define BUF_SZ 512

#if PRINT_DBG
#include <stdio.h>
#define D_PRINT(s) printf("DBG: " #s "\n")
#define D_PRINT_N(s, a) printf("DBG: " #s "\n", #a)
#else
#define D_PRINT(s)
#define D_PRINT_N(s, a)
#endif

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <IPHlpApi.h>
#include <winerror.h>

#pragma comment(lib, "Ws2_32.lib")

namespace Network {
  static bool WINSOCK_INIT = false;
  static WSADATA wsaData = {0};

  static WSADATA Winsock_Init() {
    if (!WINSOCK_INIT) {
      int res = WSAStartup(MAKEWORD(2, 2), &wsaData);

      if (res == 0) {
        WINSOCK_INIT = true;
        D_PRINT("Winsock: Initialized.");
      } else
        D_PRINT("Winsock: Not initialized!");
    }

    return wsaData;
  }

  static int Winsock_Exit() {
    if (WINSOCK_INIT) {
      WINSOCK_INIT = false;
      return WSACleanup();
    } else {
      D_PRINT("Winsock: Won't cleanup, wasn't initialized.");
      return ERR;
    }
  }
}

// Defines
#define EISCONN WSAEISCONN
#define EINPROGRESS WSAEINPROGRESS
#define EALREADY WSAEALREADY
#define ENETDOWN WSAENETDOWN
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EAGAIN EWOULDBLOCK

#else

// Unix includes
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define SD_BOTH 2

#endif

typedef struct {
  addrinfo hints;
  addrinfo* res;
} addr_t;

#endif
