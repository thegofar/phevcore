
#define _WIN32_WINNT 0x0501


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef __XTENSA__
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/netif.h"

#define TCP_READ lwip_read
#define TCP_WRITE lwip_write
#define TCP_CONNECT lwip_connect
#define TCP_SOCKET lwip_socket
#define TCP_HTONS PP_HTONS

#endif
#include "phev_tcpip.h"
#include "logger.h"
#ifdef _WIN32
#define TCP_READ recv
#define TCP_WRITE send
#define TCP_CONNECT connect
#define TCP_SOCKET socket
#define TCP_HTONS htons
#define TCP_READ_TIMEOUT 1000

#elif __linux__
#define TCP_READ read
#define TCP_WRITE write
#define TCP_CONNECT connect
#define TCP_SOCKET socket
#define TCP_HTONS htons
#define TCP_READ_TIMEOUT 1000

#endif


const static char *APP_TAG = "PHEV_TCPIP";

static void my_ms_to_timeval(int timeout_ms, struct timeval *tv)
{
    tv->tv_sec = timeout_ms / 1000;
    tv->tv_usec = (timeout_ms - (tv->tv_sec * 1000)) * 1000;
}

static int tcp_poll_read(int soc, int timeout_ms)
{
    int ret;
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(soc, &readset);
    struct timeval timeout;
    my_ms_to_timeval(timeout_ms, &timeout);
    ret = select(soc + 1, &readset, NULL, NULL, &timeout);
    return ret;
}
static int tcp_read(int soc, uint8_t *buffer, int len, int timeout_ms)
{
    int poll = -1;
    if ((poll = tcp_poll_read(soc, timeout_ms)) <= 0)
    {
        return poll;
    }
#ifdef _WIN32
    int read_len = TCP_READ(soc, buffer, len, 0);
#else
    int read_len = TCP_READ(soc, buffer, len);
#endif
    if (read_len == 0)
    {
        return -1;
    }
    hexdump("TCP", buffer, read_len, 0);
    return read_len;
}
#ifdef _WIN32

int phev_tcpClientConnectSocket(const char *host, uint16_t port)
{
    LOG_V(APP_TAG, "START - connectSocket");
    LOG_D(APP_TAG, "Host %s, Port %d", host, port);

    if (host == NULL)
    {
        LOG_E(APP_TAG, "Host not set");
        return -1;
    }
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(host, "8080", &hints, &result);
    if (iResult != 0)
    {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }
    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
                               ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET)
        {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR)
        {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET)
    {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    LOG_V(APP_TAG, "END - connectSocket");

    return ConnectSocket;
}
#else
int phev_tcpClientConnectSocket(const char *host, uint16_t port)
{
    LOG_V(APP_TAG, "START - connectSocket");
    LOG_D(APP_TAG, "Host %s, Port %d", host, port);

    if (host == NULL)
    {
        LOG_E(APP_TAG, "Host not set");
        return -1;
    }
    struct sockaddr_in addr;
    /* set up address to connect to */
    memset(&addr, 0, sizeof(addr));
    //addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = TCP_HTONS(port);
    addr.sin_addr.s_addr = inet_addr(host);

    LOG_I(APP_TAG, "Host %s Port %d", host, port);

    int sock = TCP_SOCKET(AF_INET, SOCK_STREAM, 0);

    if (sock == -1)
    {
        LOG_E(APP_TAG, "Failed to open socket");

        return -1;
    }
    int ret = TCP_CONNECT(sock, (struct sockaddr *)(&addr), sizeof(addr));
    if (ret == -1)
    {
        LOG_E(APP_TAG, "Failed to connect");

        return -1;
    }

    LOG_I(APP_TAG, "Connected to host %s port %d", host, port);

    //global_sock = sock;
    LOG_V(APP_TAG, "END - connectSocket");

    return sock;
}
#endif
int phev_tcpClientRead(int soc, uint8_t *buf, size_t len)
{
    LOG_V(APP_TAG, "START - read");

    int num = tcp_read(soc, buf, len, TCP_READ_TIMEOUT);

    LOG_D(APP_TAG, "Read %d bytes from tcp stream", num);
    LOG_BUFFER_HEXDUMP(APP_TAG, buf, num, LOG_DEBUG);

    LOG_V(APP_TAG, "END - read");

    return num;
}
int phev_tcpClientWrite(int soc, uint8_t *buf, size_t len)
{
    LOG_V(APP_TAG, "START - write");
#ifdef _WIN32
    int num = TCP_WRITE(soc, buf, len, 0);
#else
    int num = TCP_WRITE(soc, buf, len);
#endif
    LOG_D(APP_TAG, "Wriiten %d bytes from tcp stream", num);

    LOG_V(APP_TAG, "END - write");

    return num;
}
