#pragma once

#include <stdint.h>
#include <winsock2.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 27015
#define MAX_PACKET_SIZE 4096

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
    MSG_GAME_START
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

int ConnectServer(void);
void DisconnectServer(void);
void ConnectStopReceive(void);

int ConnectSend(uint8_t type, const uint8_t* data, uint32_t length);
int ConnectReceive(uint8_t* type, uint8_t* data, uint32_t* length);

const char* ConnectGetError(void);