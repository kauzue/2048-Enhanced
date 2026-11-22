#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "room.h"
#include "game.h"

#define MAX_ROOMS 128

#define ROOM_WAITING 0
#define ROOM_PLAYING 1
#define ROOM_RESULT 2

#define RESULT_NORMAL 0
#define RESULT_GOAL 1
#define RESULT_DISCONNECT 2

typedef struct {
    int active;

    char code[6];

    uint8_t mode;
    uint8_t phase;
    uint8_t modeSelected;

    Client* players[2];

    char names[2][MAX_PLAYER_NAME + 1];

    uint8_t ready[2];
    uint8_t restart[2];

    uint32_t gameNumber;

    GameState games[2];

    LARGE_INTEGER gameStart;
    LARGE_INTEGER frequency;

    uint8_t winner;
    uint8_t resultReason;
} Room;

static Room rooms[MAX_ROOMS];

static CRITICAL_SECTION roomLock;

static int roomLockReady = 0;

static void writeU32(uint8_t* output, uint32_t value)
{
    output[0] = (uint8_t)(value >> 24);
    output[1] = (uint8_t)(value >> 16);
    output[2] = (uint8_t)(value >> 8);
    output[3] = (uint8_t)value;
}

static uint32_t getElapsedMs(const Room* room)
{
    LARGE_INTEGER now;
    uint64_t ticks;

    QueryPerformanceCounter(&now);

    ticks = (uint64_t)(
        now.QuadPart - room->gameStart.QuadPart
        );

    if (room->frequency.QuadPart <= 0) {
        return 0;
    }

    return (uint32_t)(
        ticks * 1000u /
        (uint64_t)room->frequency.QuadPart
        );
}

static uint8_t getBoardSize(uint8_t mode)
{
    return mode == MODE_SURVIVAL ? 3 : 4;
}

static uint8_t getTargetExponent(uint8_t mode)
{
    if (mode == MODE_2048) {
        return 11;
    }

    if (mode == MODE_4096) {
        return 12;
    }

    return 0;
}

static void copyString(
    char* target,
    size_t size,
    const char* source)
{
    size_t length;

    if (target == NULL ||
        source == NULL ||
        size == 0) {
        return;
    }

    length = strlen(source);

    if (length >= size) {
        length = size - 1;
    }

    memcpy(target, source, length);

    target[length] = '\0';
}

static void sendError(Client* client, const char* message)
{
    if (client != NULL && client->connected) {
        SendPacket(
            client,
            MSG_ERROR,
            (const uint8_t*)message,
            (uint32_t)strlen(message)
        );
    }
}

static void rejectJoin(Client* client, const char* message)
{
    if (client->joinFailures < 5) {
        client->joinFailures++;
    }

    if (client->joinFailures >= 5) {
        const char* errorMessage =
            "접속 실패 횟수가 초과되었습니다. 다시 연결한 뒤 시도하세요.";

        SendPacket(
            client,
            MSG_JOIN_FAILED,
            (const uint8_t*)errorMessage,
            (uint32_t)strlen(errorMessage)
        );
    }
    else {
        SendPacket(
            client,
            MSG_JOIN_FAILED,
            (const uint8_t*)message,
            (uint32_t)strlen(message)
        );
    }
}

static int findRoomSlot(void)
{
    int i;

    for (i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) {
            return i;
        }
    }

    return -1;
}

static int codeExists(const char* code)
{
    int i;

    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active &&
            strcmp(rooms[i].code, code) == 0) {

            return 1;
        }
    }

    return 0;
}

static void generateRoomCode(char* code)
{
    unsigned int value;

    do {
        value = (unsigned int)(rand() % 100000);

        snprintf(
            code,
            6,
            "%05u",
            value
        );

    } while (codeExists(code));
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
        copyString(
            client->name,
            sizeof(client->name),
            "PLAYER"
        );
    }
}

static void sendRoomState(Room* room)
{
    uint8_t data[128];

    uint32_t offset = 0;

    int i;

    memcpy(
        data + offset,
        room->code,
        5
    );

    offset += 5;

    data[offset++] = room->mode;
    data[offset++] = room->phase;
    data[offset++] = room->modeSelected;

    data[offset++] = room->restart[0];
    data[offset++] = room->restart[1];

    for (i = 0; i < 2; i++) {
        uint8_t nameLength =
            (uint8_t)strlen(room->names[i]);

        int connected =
            room->players[i] != NULL &&
            room->players[i]->connected;

        data[offset++] = (uint8_t)connected;
        data[offset++] = room->ready[i];
        data[offset++] = nameLength;

        memcpy(
            data + offset,
            room->names[i],
            nameLength
        );

        offset += nameLength;
    }

    for (i = 0; i < 2; i++) {
        if (room->players[i] != NULL &&
            room->players[i]->connected) {

            SendPacket(
                room->players[i],
                MSG_ROOM_STATE,
                data,
                offset
            );
        }
    }
}

static void sendGameState(Room* room)
{
    uint8_t data[80];

    uint32_t offset = 0;

    int i, y, x;

    writeU32(
        data + offset,
        room->gameNumber
    );

    offset += 4;

    data[offset++] = room->mode;

    for (i = 0; i < 2; i++) {
        GameState* game = &room->games[i];

        int connected =
            room->players[i] != NULL &&
            room->players[i]->connected;

        data[offset++] = (uint8_t)connected;
        data[offset++] = game->status;

        writeU32(
            data + offset,
            game->score
        );

        offset += 4;

        data[offset++] =
            GameGetMaxExponent(game);

        writeU32(
            data + offset,
            game->elapsedMs
        );

        offset += 4;

        for (y = 0; y < 4; y++) {
            for (x = 0; x < 4; x++) {
                data[offset++] =
                    y < game->size && x < game->size
                    ? game->board[y][x]
                    : 0;
            }
        }
    }

    for (i = 0; i < 2; i++) {
        if (room->players[i] != NULL &&
            room->players[i]->connected) {

            SendPacket(
                room->players[i],
                MSG_GAME_STATE,
                data,
                offset
            );
        }
    }
}

static void sendRestartState(Room* room)
{
    uint8_t data[2];

    data[0] = room->restart[0];
    data[1] = room->restart[1];

    if (room->players[0] != NULL &&
        room->players[0]->connected) {

        SendPacket(
            room->players[0],
            MSG_RESTART_READY,
            data,
            sizeof(data)
        );
    }

    if (room->players[1] != NULL &&
        room->players[1]->connected) {

        SendPacket(
            room->players[1],
            MSG_RESTART_READY,
            data,
            sizeof(data)
        );
    }
}

static void releaseRoom(Room* room)
{
    int i;

    for (i = 0; i < 2; i++) {
        if (room->players[i] != NULL) {
            room->players[i]->roomIndex = -1;
            room->players[i]->slot = -1;
        }
    }

    memset(
        room,
        0,
        sizeof(*room)
    );
}

static void closeRoom(Room* room)
{
    uint8_t empty = 0;

    int i;

    for (i = 0; i < 2; i++) {
        Client* player = room->players[i];

        if (player != NULL) {
            if (player->connected) {
                SendPacket(
                    player,
                    MSG_ROOM_CLOSED,
                    &empty,
                    0
                );
            }

            player->roomIndex = -1;
            player->slot = -1;
        }
    }

    memset(
        room,
        0,
        sizeof(*room)
    );
}

static void notifyPlayerLeft(Room* room, int slot)
{
    uint8_t data = (uint8_t)slot;

    int other = 1 - slot;

    if (room->players[other] != NULL &&
        room->players[other]->connected) {

        SendPacket(
            room->players[other],
            MSG_PLAYER_LEFT,
            &data,
            sizeof(data)
        );
    }
}

static void sendGameStart(Room* room)
{
    uint8_t data[4];

    int i;

    writeU32(
        data,
        room->gameNumber
    );

    for (i = 0; i < 2; i++) {
        if (room->players[i] != NULL &&
            room->players[i]->connected) {

            SendPacket(
                room->players[i],
                MSG_GAME_START,
                data,
                sizeof(data)
            );
        }
    }
}

static void startGame(Room* room)
{
    uint8_t size = getBoardSize(room->mode);

    LARGE_INTEGER seed;

    int i;

    room->phase = ROOM_PLAYING;

    room->gameNumber++;

    room->restart[0] = 0;
    room->restart[1] = 0;

    room->ready[0] = 0;
    room->ready[1] = 0;

    QueryPerformanceCounter(&seed);

    srand(
        (unsigned int)(
            seed.QuadPart ^ GetTickCount()
            )
    );

    for (i = 0; i < 2; i++) {
        GameInit(
            &room->games[i],
            size
        );
    }

    QueryPerformanceFrequency(&room->frequency);
    QueryPerformanceCounter(&room->gameStart);

    sendGameStart(room);
    sendGameState(room);
}

static void sendGameResult(
    Room* room,
    uint8_t winner,
    uint8_t reason)
{
    uint8_t data[23];

    uint32_t offset = 0;

    int i;

    room->phase = ROOM_RESULT;

    room->winner = winner;
    room->resultReason = reason;

    room->restart[0] = 0;
    room->restart[1] = 0;

    data[offset++] = room->mode;
    data[offset++] = winner;
    data[offset++] = reason;

    for (i = 0; i < 2; i++) {
        data[offset++] = room->games[i].status;

        data[offset++] =
            GameGetMaxExponent(&room->games[i]);

        writeU32(
            data + offset,
            room->games[i].score
        );

        offset += 4;

        writeU32(
            data + offset,
            room->games[i].elapsedMs
        );

        offset += 4;
    }

    for (i = 0; i < 2; i++) {
        if (room->players[i] != NULL &&
            room->players[i]->connected) {

            SendPacket(
                room->players[i],
                MSG_GAME_RESULT,
                data,
                offset
            );
        }
    }

    sendGameState(room);
}

static void finishIfBothOver(Room* room)
{
    uint8_t maximum0;
    uint8_t maximum1;

    uint8_t winner = 2;

    if (room->games[0].status == GAME_ACTIVE ||
        room->games[1].status == GAME_ACTIVE) {

        sendGameState(room);

        return;
    }

    maximum0 = GameGetMaxExponent(&room->games[0]);
    maximum1 = GameGetMaxExponent(&room->games[1]);

    if (maximum0 > maximum1) {
        winner = 0;
    }
    else if (maximum1 > maximum0) {
        winner = 1;
    }
    else if (room->mode == MODE_SURVIVAL) {
        if (room->games[0].elapsedMs <
            room->games[1].elapsedMs) {

            winner = 0;
        }
        else if (room->games[1].elapsedMs <
            room->games[0].elapsedMs) {

            winner = 1;
        }
    }

    sendGameResult(
        room,
        winner,
        RESULT_NORMAL
    );
}

static void handlePlayerGameOver(Room* room, int slot)
{
    int i;

    uint8_t data = (uint8_t)slot;

    room->games[slot].status = GAME_OVER;

    if (room->mode == MODE_SURVIVAL) {
        room->games[slot].elapsedMs =
            getElapsedMs(room);
    }

    for (i = 0; i < 2; i++) {
        if (room->players[i] != NULL &&
            room->players[i]->connected) {

            SendPacket(
                room->players[i],
                MSG_GAME_OVER,
                &data,
                sizeof(data)
            );
        }
    }

    finishIfBothOver(room);
}

static void handleMove(
    Room* room,
    Client* client,
    const uint8_t* data,
    uint32_t length)
{
    int slot = client->slot;

    uint8_t target;

    if (room->phase != ROOM_PLAYING ||
        slot < 0 ||
        slot > 1 ||
        length != 1) {

        return;
    }

    if (room->games[slot].status != GAME_ACTIVE ||
        data[0] > MOVE_DOWN) {

        return;
    }

    if (!GameMove(
        &room->games[slot],
        data[0])) {

        return;
    }

    target = getTargetExponent(room->mode);

    if (target > 0 &&
        GameGetMaxExponent(&room->games[slot]) >= target) {

        room->games[slot].status = GAME_WON;
        room->games[1 - slot].status = GAME_LOST;

        sendGameResult(
            room,
            (uint8_t)slot,
            RESULT_GOAL
        );

        return;
    }

    if (!GameHasMoves(&room->games[slot])) {
        handlePlayerGameOver(
            room,
            slot
        );

        return;
    }

    sendGameState(room);
}

static void handleCreateRoom(
    Client* client,
    const uint8_t* data,
    uint32_t length)
{
    int index;

    Room* room;

    uint8_t response[6];

    if (!client->connected ||
        client->roomIndex >= 0) {

        sendError(
            client,
            "이미 방에 참가 중이거나 연결 상태가 올바르지 않습니다."
        );

        return;
    }

    if (length > MAX_PLAYER_NAME) {
        sendError(
            client,
            "플레이어 이름이 너무 깁니다."
        );

        return;
    }

    setClientName(
        client,
        data,
        length
    );

    index = findRoomSlot();

    if (index < 0) {
        sendError(
            client,
            "서버에 생성 가능한 방이 없습니다."
        );

        return;
    }

    room = &rooms[index];

    memset(
        room,
        0,
        sizeof(*room)
    );

    room->phase = ROOM_WAITING;
    room->mode = MODE_SURVIVAL;

    QueryPerformanceFrequency(
        &room->frequency
    );

    generateRoomCode(room->code);

    room->active = 1;

    room->players[0] = client;

    copyString(
        room->names[0],
        sizeof(room->names[0]),
        client->name
    );

    client->roomIndex = index;
    client->slot = 0;

    memcpy(
        response,
        room->code,
        5
    );

    response[5] = 0;

    if (!SendPacket(
        client,
        MSG_ROOM_CREATED,
        response,
        sizeof(response))) {

        releaseRoom(room);

        return;
    }

    sendRoomState(room);
}

static void handleJoinRoom(
    Client* client,
    const uint8_t* data,
    uint32_t length)
{
    char code[6];

    Room* room = NULL;

    int i;

    uint8_t response[6];
    uint8_t nameLength;

    if (length < 6 ||
        client->roomIndex >= 0) {

        rejectJoin(
            client,
            "잘못된 접속 요청입니다."
        );

        return;
    }

    memcpy(
        code,
        data,
        5
    );

    code[5] = '\0';

    for (i = 0; i < 5; i++) {
        if (code[i] < '0' ||
            code[i] > '9') {

            rejectJoin(
                client,
                "방 코드는 숫자 5자리여야 합니다."
            );

            return;
        }
    }

    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active &&
            strcmp(rooms[i].code, code) == 0) {

            room = &rooms[i];

            break;
        }
    }

    if (room == NULL) {
        rejectJoin(
            client,
            "존재하지 않는 방 코드입니다."
        );

        return;
    }

    if (room->phase != ROOM_WAITING) {
        rejectJoin(
            client,
            "이미 게임이 시작된 방입니다."
        );

        return;
    }

    if (room->players[1] != NULL) {
        rejectJoin(
            client,
            "이미 참가자가 있는 방입니다."
        );

        return;
    }

    nameLength = data[5];

    if (nameLength > MAX_PLAYER_NAME ||
        length != (uint32_t)(6 + nameLength)) {

        rejectJoin(
            client,
            "잘못된 플레이어 정보입니다."
        );

        return;
    }

    setClientName(
        client,
        data + 6,
        nameLength
    );

    client->joinFailures = 0;

    room->players[1] = client;

    copyString(
        room->names[1],
        sizeof(room->names[1]),
        client->name
    );

    room->ready[1] = 0;

    client->roomIndex = (int)(room - rooms);
    client->slot = 1;

    memcpy(
        response,
        room->code,
        5
    );

    response[5] = 1;

    if (!SendPacket(
        client,
        MSG_JOIN_SUCCESS,
        response,
        sizeof(response))) {

        room->players[1] = NULL;

        room->names[1][0] = '\0';

        client->roomIndex = -1;
        client->slot = -1;

        return;
    }

    sendRoomState(room);
}

static void handleLeaveGame(
    Room* room,
    Client* client,
    int slot)
{
    int other = 1 - slot;

    room->players[slot] = NULL;

    client->roomIndex = -1;
    client->slot = -1;

    room->games[slot].status = GAME_LEFT;
    room->games[other].status = GAME_WON;

    if (room->mode == MODE_SURVIVAL) {
        uint32_t elapsed = getElapsedMs(room);

        room->games[slot].elapsedMs = elapsed;
        room->games[other].elapsedMs = elapsed;
    }

    room->restart[0] = 0;
    room->restart[1] = 0;

    notifyPlayerLeft(room, slot);

    sendGameResult(
        room,
        (uint8_t)other,
        RESULT_DISCONNECT
    );

    if (room->players[0] == NULL &&
        room->players[1] == NULL) {

        releaseRoom(room);
    }
}

static void handleReturnMain(Client* client)
{
    Room* room;

    int slot;

    if (client->roomIndex < 0 ||
        client->roomIndex >= MAX_ROOMS) {

        return;
    }

    room = &rooms[client->roomIndex];

    slot = client->slot;

    if (!room->active ||
        slot < 0 ||
        slot > 1) {

        client->roomIndex = -1;
        client->slot = -1;

        return;
    }

    if (room->phase == ROOM_WAITING && slot == 0) {
        closeRoom(room);

        return;
    }

    if (room->phase == ROOM_WAITING && slot == 1) {
        room->players[1] = NULL;
        room->ready[1] = 0;
        room->names[1][0] = '\0';

        client->roomIndex = -1;
        client->slot = -1;

        notifyPlayerLeft(room, slot);

        sendRoomState(room);

        return;
    }

    if (room->phase == ROOM_PLAYING) {
        handleLeaveGame(
            room,
            client,
            slot
        );

        return;
    }

    if (room->phase == ROOM_RESULT) {
        room->players[slot] = NULL;

        client->roomIndex = -1;
        client->slot = -1;

        room->restart[0] = 0;
        room->restart[1] = 0;

        notifyPlayerLeft(room, slot);

        sendRestartState(room);

        if (room->players[0] == NULL &&
            room->players[1] == NULL) {

            releaseRoom(room);
        }
    }
}

static void handleRestart(Room* room)
{
    if (room->phase != ROOM_RESULT ||
        room->players[0] == NULL ||
        room->players[1] == NULL) {

        return;
    }

    if (!room->players[0]->connected ||
        !room->players[1]->connected) {

        return;
    }

    if (room->restart[0] &&
        room->restart[1]) {

        startGame(room);
    }
    else {
        sendRestartState(room);
    }
}

void RoomInit(void)
{
    memset(
        rooms,
        0,
        sizeof(rooms)
    );

    InitializeCriticalSection(&roomLock);

    roomLockReady = 1;

    srand(
        (unsigned int)time(NULL)
    );
}

void RoomHandleMessage(
    Client* client,
    uint8_t type,
    const uint8_t* data,
    uint32_t length)
{
    Room* room;

    int slot;

    if (!roomLockReady ||
        client == NULL ||
        !client->connected) {

        return;
    }

    EnterCriticalSection(&roomLock);

    switch (type) {
    case MSG_CREATE_ROOM:
        handleCreateRoom(
            client,
            data,
            length
        );
        break;

    case MSG_JOIN_ROOM:
        handleJoinRoom(
            client,
            data,
            length
        );
        break;

    case MSG_MODE_CHANGE:
        if (client->roomIndex >= 0 &&
            client->slot == 0 &&
            length == 1 &&
            data[0] <= MODE_INFINITE) {

            room = &rooms[client->roomIndex];

            if (room->active &&
                room->phase == ROOM_WAITING) {

                room->mode = data[0];
                room->modeSelected = 1;

                sendRoomState(room);
            }
        }
        break;

    case MSG_READY:
    case MSG_UNREADY:
        if (client->roomIndex >= 0 &&
            client->slot == 1) {

            room = &rooms[client->roomIndex];

            if (room->active &&
                room->phase == ROOM_WAITING) {

                room->ready[1] =
                    type == MSG_READY ? 1 : 0;

                sendRoomState(room);
            }
        }
        break;

    case MSG_START_GAME:
        if (client->roomIndex >= 0 &&
            client->slot == 0) {

            room = &rooms[client->roomIndex];

            if (room->active &&
                room->phase == ROOM_WAITING &&
                room->modeSelected &&
                room->players[0] != NULL &&
                room->players[1] != NULL &&
                room->players[0]->connected &&
                room->players[1]->connected &&
                room->ready[1]) {

                startGame(room);
            }
            else {
                sendError(
                    client,
                    "모드 선택, 참가자 접속 및 준비가 완료되어야 시작할 수 있습니다."
                );
            }
        }
        break;

    case MSG_MOVE:
        if (client->roomIndex >= 0) {
            room = &rooms[client->roomIndex];

            if (room->active) {
                handleMove(
                    room,
                    client,
                    data,
                    length
                );
            }
        }
        break;

    case MSG_RESTART_REQUEST:
    case MSG_RESTART_CANCEL:
        if (client->roomIndex >= 0) {
            room = &rooms[client->roomIndex];

            slot = client->slot;

            if (room->active &&
                room->phase == ROOM_RESULT &&
                slot >= 0 &&
                slot <= 1) {

                room->restart[slot] =
                    type == MSG_RESTART_REQUEST ? 1 : 0;

                handleRestart(room);
            }
        }
        break;

    case MSG_RETURN_MAIN:
        handleReturnMain(client);
        break;

    case MSG_DISCONNECT:
        break;

    default:
        sendError(
            client,
            "지원하지 않는 메시지입니다."
        );
        break;
    }

    LeaveCriticalSection(&roomLock);
}

void RoomDisconnect(Client* client)
{
    Room* room;

    int slot;

    if (!roomLockReady ||
        client == NULL) {

        return;
    }

    EnterCriticalSection(&roomLock);

    client->connected = 0;

    if (client->roomIndex < 0 ||
        client->roomIndex >= MAX_ROOMS) {

        LeaveCriticalSection(&roomLock);

        return;
    }

    room = &rooms[client->roomIndex];

    slot = client->slot;

    if (!room->active ||
        slot < 0 ||
        slot > 1) {

        client->roomIndex = -1;
        client->slot = -1;

        LeaveCriticalSection(&roomLock);

        return;
    }

    if (room->phase == ROOM_WAITING && slot == 0) {
        closeRoom(room);

        LeaveCriticalSection(&roomLock);

        return;
    }

    if (room->phase == ROOM_WAITING && slot == 1) {
        room->players[1] = NULL;
        room->ready[1] = 0;
        room->names[1][0] = '\0';

        client->roomIndex = -1;
        client->slot = -1;

        notifyPlayerLeft(room, slot);

        sendRoomState(room);
    }
    else if (room->phase == ROOM_PLAYING) {
        handleLeaveGame(
            room,
            client,
            slot
        );
    }
    else if (room->phase == ROOM_RESULT) {
        room->players[slot] = NULL;

        client->roomIndex = -1;
        client->slot = -1;

        room->restart[0] = 0;
        room->restart[1] = 0;

        notifyPlayerLeft(room, slot);

        sendRestartState(room);

        if (room->players[0] == NULL &&
            room->players[1] == NULL) {

            releaseRoom(room);
        }
    }

    LeaveCriticalSection(&roomLock);
}