#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "connect.h"
#include "room.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

static int sendAll(
    SOCKET socket,
    const char* data,
    int length)
{
    int sent = 0;

    while (sent < length) {
        int result = send(
            socket,
            data + sent,
            length - sent,
            0
        );

        if (result == SOCKET_ERROR || result == 0) {
            return 0;
        }

        sent += result;
    }

    return 1;
}

static int receiveAll(
    SOCKET socket,
    char* data,
    int length)
{
    int received = 0;

    while (received < length) {
        int result = recv(
            socket,
            data + received,
            length - received,
            0
        );

        if (result == SOCKET_ERROR || result == 0) {
            return 0;
        }

        received += result;
    }

    return 1;
}

int SendPacket(
    Client* client,
    uint8_t type,
    const uint8_t* data,
    uint32_t length)
{
    uint32_t frameLength;
    uint32_t networkLength;

    char header[4];

    int result;

    if (client == NULL ||
        !client->connected ||
        length > MAX_PACKET_SIZE - 1) {
        return 0;
    }

    frameLength = length + 1;

    networkLength = htonl(frameLength);

    memcpy(
        header,
        &networkLength,
        sizeof(header)
    );

    EnterCriticalSection(&client->sendLock);

    result = sendAll(
        client->socket,
        header,
        sizeof(header)
    );

    if (result) {
        result = sendAll(
            client->socket,
            (const char*)&type,
            1
        );
    }

    if (result && length > 0) {
        result = sendAll(
            client->socket,
            (const char*)data,
            (int)length
        );
    }

    LeaveCriticalSection(&client->sendLock);

    return result;
}

int ReceivePacket(
    SOCKET socket,
    uint8_t* type,
    uint8_t* data,
    uint32_t* length)
{
    uint32_t networkLength;
    uint32_t frameLength;

    if (type == NULL ||
        data == NULL ||
        length == NULL) {
        return 0;
    }

    if (!receiveAll(
        socket,
        (char*)&networkLength,
        sizeof(networkLength))) {
        return 0;
    }

    frameLength = ntohl(networkLength);

    if (frameLength < 1 ||
        frameLength > MAX_PACKET_SIZE) {
        return 0;
    }

    if (!receiveAll(
        socket,
        (char*)type,
        1)) {
        return 0;
    }

    *length = frameLength - 1;

    if (*length > 0) {
        if (!receiveAll(
            socket,
            (char*)data,
            (int)*length)) {
            return 0;
        }
    }

    return 1;
}

static IN_ADDR GetDefaultMyIP(void)
{
    IN_ADDR address;

    ULONG bufferSize = 15000;

    ULONG flags =
        GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER;

    ULONG result;

    DWORD interfaceIndex = 0;

    SOCKADDR_IN destination;

    PIP_ADAPTER_ADDRESSES adapters = NULL;
    PIP_ADAPTER_ADDRESSES adapter;

    memset(&address, 0, sizeof(address));
    memset(&destination, 0, sizeof(destination));

    destination.sin_family = AF_INET;

    InetPtonA(
        AF_INET,
        "1.1.1.1",
        &destination.sin_addr
    );

    if (GetBestInterfaceEx(
        (SOCKADDR*)&destination,
        &interfaceIndex) != NO_ERROR) {

        interfaceIndex = 0;
    }

    result = GetAdaptersAddresses(
        AF_INET,
        flags,
        NULL,
        NULL,
        &bufferSize
    );

    if (result != ERROR_BUFFER_OVERFLOW) {
        return address;
    }

    adapters = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);

    if (adapters == NULL) {
        return address;
    }

    result = GetAdaptersAddresses(
        AF_INET,
        flags,
        NULL,
        adapters,
        &bufferSize
    );

    if (result != NO_ERROR) {
        free(adapters);
        return address;
    }

    if (interfaceIndex != 0) {
        for (adapter = adapters;
            adapter != NULL;
            adapter = adapter->Next) {

            PIP_ADAPTER_UNICAST_ADDRESS unicast;

            if (adapter->OperStatus != IfOperStatusUp ||
                adapter->IfIndex != interfaceIndex) {
                continue;
            }

            for (unicast = adapter->FirstUnicastAddress;
                unicast != NULL;
                unicast = unicast->Next) {

                struct sockaddr_in* ipv4;

                if (unicast->Address.lpSockaddr == NULL ||
                    unicast->Address.lpSockaddr->sa_family != AF_INET) {
                    continue;
                }

                ipv4 = (struct sockaddr_in*)
                    unicast->Address.lpSockaddr;

                if (ipv4->sin_addr.s_addr != INADDR_ANY) {
                    address = ipv4->sin_addr;

                    free(adapters);
                    return address;
                }
            }
        }
    }

    free(adapters);

    return address;
}

static void setClientName(
    Client* client,
    const uint8_t* data,
    uint32_t length)
{
    uint32_t count = length;

    if (count > MAX_PLAYER_NAME) {
        count = MAX_PLAYER_NAME;
    }

    if (count > 0 && data != NULL) {
        memcpy(
            client->name,
            data,
            count
        );
    }

    client->name[count] = '\0';

    if (client->name[0] == '\0') {
        memcpy(
            client->name,
            "PLAYER",
            sizeof("PLAYER")
        );
    }
}

static DWORD WINAPI clientThread(LPVOID parameter)
{
    Client* client = (Client*)parameter;

    uint8_t type;
    uint8_t data[MAX_PACKET_SIZE];

    uint32_t length;

    int namePrinted = 0;

    while (ReceivePacket(
        client->socket,
        &type,
        data,
        &length)) {

        if (type == MSG_DISCONNECT) {
            break;
        }

        if (type == MSG_SET_NAME) {
            setClientName(
                client,
                data,
                length
            );

            if (!namePrinted) {
                printf(
                    "%s가 서버에 연결되었습니다.\n",
                    client->name
                );

                namePrinted = 1;
            }

            continue;
        }

        RoomHandleMessage(
            client,
            type,
            data,
            length
        );

        if (!namePrinted &&
            client->roomIndex >= 0) {

            printf(
                "%s님이 서버에 연결되었습니다.\n",
                client->name
            );

            namePrinted = 1;
        }
    }

    RoomDisconnect(client);

    shutdown(client->socket, SD_BOTH);

    closesocket(client->socket);

    printf(
        "%s님의 연결이 끊어졌습니다.\n",
        client->name
    );

    DeleteCriticalSection(&client->sendLock);

    free(client);

    return 0;
}

void serverRun(void)
{
    WSADATA data;

    SOCKET listenSocket;

    struct sockaddr_in address;

    IN_ADDR serverIP;

    char serverIPText[16];

    int reuse = 1;
    int result;

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return;
    }

    RoomInit();

    listenSocket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    setsockopt(
        listenSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        (const char*)&reuse,
        sizeof(reuse)
    );

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(SERVER_PORT);

    result = bind(
        listenSocket,
        (struct sockaddr*)&address,
        sizeof(address)
    );

    if (result == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    result = listen(
        listenSocket,
        SOMAXCONN
    );

    if (result == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    serverIP = GetDefaultMyIP();

    if (serverIP.s_addr == INADDR_ANY ||
        InetNtopA(
            AF_INET,
            &serverIP,
            serverIPText,
            sizeof(serverIPText)
        ) == NULL) {

        memcpy(
            serverIPText,
            "127.0.0.1",
            sizeof("127.0.0.1")
        );
    }

    printf(
        "%s:%d에서 서버 실행중\n",
        serverIPText,
        SERVER_PORT
    );

    while (1) {
        SOCKET clientSocket;

        struct sockaddr_in clientAddress;

        int addressLength = sizeof(clientAddress);

        Client* client;

        HANDLE thread;
        DWORD threadId;

        clientSocket = accept(
            listenSocket,
            (struct sockaddr*)&clientAddress,
            &addressLength
        );

        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        client = (Client*)calloc(
            1,
            sizeof(Client)
        );

        if (client == NULL) {
            closesocket(clientSocket);
            continue;
        }

        client->socket = clientSocket;
        client->connected = 1;
        client->roomIndex = -1;
        client->slot = -1;

        memcpy(
            client->name,
            "PLAYER",
            sizeof("PLAYER")
        );

        if (InetNtopA(
            AF_INET,
            &clientAddress.sin_addr,
            client->ip,
            sizeof(client->ip)) == NULL) {

            memcpy(
                client->ip,
                "UNKNOWN",
                sizeof("UNKNOWN")
            );
        }

        InitializeCriticalSection(
            &client->sendLock
        );

        thread = CreateThread(
            NULL,
            0,
            clientThread,
            client,
            0,
            &threadId
        );

        if (thread == NULL) {
            DeleteCriticalSection(
                &client->sendLock
            );

            closesocket(clientSocket);
            free(client);

            continue;
        }

        CloseHandle(thread);
    }
}