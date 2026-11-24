#pragma once

#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

#define MAX_PACKET_SIZE 4096
#define MAX_PLAYER_NAME 16

extern int SERVER_PORT;

enum {
    MSG_CREATE_ROOM = 1,
    MSG_ROOM_CREATED,
    MSG_JOIN_ROOM,
    MSG_JOIN_SUCCESS,
    MSG_JOIN_FAILED,
    MSG_MODE_CHANGE,
    MSG_READY,
    MSG_UNREADY,
    MSG_START_GAME,
    MSG_MOVE,
    MSG_GAME_STATE,
    MSG_GAME_OVER,
    MSG_GAME_RESULT,
    MSG_RESTART_REQUEST,
    MSG_RESTART_CANCEL,
    MSG_RESTART_READY,
    MSG_RETURN_MAIN,
    MSG_PLAYER_LEFT,
    MSG_ROOM_CLOSED,
    MSG_DISCONNECT,
    MSG_ERROR,
    MSG_ROOM_STATE,
    MSG_GAME_START,
    MSG_SET_NAME
};

enum {
    MODE_SURVIVAL = 0,
    MODE_2048,
    MODE_4096,
    MODE_INFINITE
};

enum {
    MOVE_LEFT = 0,
    MOVE_UP,
    MOVE_RIGHT,
    MOVE_DOWN
};

typedef struct Client {
    SOCKET socket;
    CRITICAL_SECTION sendLock;

    int connected;
    int roomIndex;
    int slot;

    uint8_t joinFailures;

    char name[MAX_PLAYER_NAME + 1];
    char ip[16];
} Client;

int SendPacket(
    Client* client,
    uint8_t type,
    const uint8_t* data,
    uint32_t length
);

int ReceivePacket(
    SOCKET socket,
    uint8_t* type,
    uint8_t* data,
    uint32_t* length
);

void serverRun(void);