#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "connect.h"

#pragma comment(lib, "Ws2_32.lib")

static SOCKET serverSocket = INVALID_SOCKET;
static CRITICAL_SECTION sendLock;

static int lockInitialized = 0;
static int winsockInitialized = 0;

static char connectError[256];

static int sendAll(SOCKET socket, const char* data, int length)
{
    int sent = 0;

    while (sent < length) {
        int result = send(socket, data + sent, length - sent, 0);

        if (result == SOCKET_ERROR || result == 0) {
            return 0;
        }

        sent += result;
    }

    return 1;
}

static int receiveAll(SOCKET socket, char* data, int length)
{
    int received = 0;

    while (received < length) {
        int result = recv(socket, data + received, length - received, 0);

        if (result == SOCKET_ERROR || result == 0) {
            return 0;
        }

        received += result;
    }

    return 1;
}

int ConnectServer(void)
{
    WSADATA data;
    struct sockaddr_in address;

    int result;
    int addressResult;
    int errorCode;

    connectError[0] = '\0';

    errorCode = WSAStartup(MAKEWORD(2, 2), &data);

    if (errorCode != 0) {
        snprintf(
            connectError,
            sizeof(connectError),
            "Winsock 초기화에 실패했습니다. 오류 코드: %d",
            errorCode
        );

        return 0;
    }

    winsockInitialized = 1;

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET) {
        snprintf(
            connectError,
            sizeof(connectError),
            "소켓 생성에 실패했습니다. 오류 코드: %d",
            WSAGetLastError()
        );

        DisconnectServer();
        return 0;
    }

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(SERVER_PORT);

    addressResult = InetPtonA(
        AF_INET,
        SERVER_IP,
        &address.sin_addr
    );

    if (addressResult != 1) {
        if (addressResult == 0) {
            snprintf(
                connectError,
                sizeof(connectError),
                "잘못된 서버 IP 주소입니다: %s",
                SERVER_IP
            );
        }
        else {
            snprintf(
                connectError,
                sizeof(connectError),
                "IP 주소 변환에 실패했습니다. 오류 코드: %d",
                WSAGetLastError()
            );
        }

        DisconnectServer();
        return 0;
    }

    result = connect(
        serverSocket,
        (struct sockaddr*)&address,
        sizeof(address)
    );

    if (result == SOCKET_ERROR) {
        errorCode = WSAGetLastError();

        snprintf(
            connectError,
            sizeof(connectError),
            "서버에 연결할 수 없습니다. IP: %s, PORT: %d, 오류 코드: %d",
            SERVER_IP,
            SERVER_PORT,
            errorCode
        );

        DisconnectServer();
        return 0;
    }

    InitializeCriticalSection(&sendLock);
    lockInitialized = 1;

    return 1;
}

void DisconnectServer(void)
{
    if (serverSocket != INVALID_SOCKET) {
        shutdown(serverSocket, SD_BOTH);
        closesocket(serverSocket);

        serverSocket = INVALID_SOCKET;
    }

    if (lockInitialized) {
        DeleteCriticalSection(&sendLock);
        lockInitialized = 0;
    }

    if (winsockInitialized) {
        WSACleanup();
        winsockInitialized = 0;
    }
}

void ConnectStopReceive(void)
{
    if (serverSocket != INVALID_SOCKET) {
        shutdown(serverSocket, SD_RECEIVE);
    }
}

int ConnectSend(uint8_t type, const uint8_t* data, uint32_t length)
{
    uint32_t frameLength;
    uint32_t networkLength;

    char header[4];

    int result;

    if (serverSocket == INVALID_SOCKET ||
        length > MAX_PACKET_SIZE - 1) {
        return 0;
    }

    frameLength = length + 1;
    networkLength = htonl(frameLength);

    memcpy(header, &networkLength, sizeof(header));

    EnterCriticalSection(&sendLock);

    result = sendAll(serverSocket, header, sizeof(header)) &&
        sendAll(serverSocket, (const char*)&type, 1);

    if (result && length > 0) {
        result = sendAll(
            serverSocket,
            (const char*)data,
            (int)length
        );
    }

    LeaveCriticalSection(&sendLock);

    return result;
}

int ConnectReceive(uint8_t* type, uint8_t* data, uint32_t* length)
{
    uint32_t networkLength;
    uint32_t frameLength;

    if (serverSocket == INVALID_SOCKET ||
        type == NULL ||
        data == NULL ||
        length == NULL) {
        return 0;
    }

    if (!receiveAll(
        serverSocket,
        (char*)&networkLength,
        sizeof(networkLength))) {
        return 0;
    }

    frameLength = ntohl(networkLength);

    if (frameLength < 1 ||
        frameLength > MAX_PACKET_SIZE) {
        return 0;
    }

    if (!receiveAll(serverSocket, (char*)type, 1)) {
        return 0;
    }

    *length = frameLength - 1;

    if (*length > 0 &&
        !receiveAll(serverSocket, (char*)data, (int)*length)) {
        return 0;
    }

    return 1;
}

const char* ConnectGetError(void)
{
    return connectError;
}