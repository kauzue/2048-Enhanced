#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <conio.h>

#include "connect.h"
#include "multy.h"
#include "system.h"

#include <windows.h>

#define NAME_LENGTH 16
#define SCREEN_MENU 0
#define SCREEN_WAIT 1
#define SCREEN_ROOM 2
#define SCREEN_GAME 3
#define SCREEN_RESULT 4
#define SCREEN_ERROR 5

#define PLAYER_ACTIVE 0
#define PLAYER_OVER 1
#define PLAYER_WON 2
#define PLAYER_LOST 3
#define PLAYER_LEFT 4

typedef struct {
    int screen;
    int slot;
    char roomCode[6];
    uint8_t mode;
    uint8_t phase;
    uint8_t modeSelected;
    uint8_t present[2];
    uint8_t ready[2];
    uint8_t restart[2];
    char names[2][NAME_LENGTH + 1];
    uint8_t status[2];
    uint32_t score[2];
    uint8_t maxExponent[2];
    uint32_t elapsedMs[2];
    uint8_t board[2][4][4];
    uint8_t winner;
    uint8_t resultReason;
    char message[160];
} MultiState;

static MultiState multiState;
static CRITICAL_SECTION stateLock;
static HANDLE receiveThreadHandle = NULL;
static volatile LONG receiveRunning = 0;

static uint8_t multiScheme = 0;

static uint32_t readU32(const uint8_t* data)
{
    uint32_t value;

    value = ((uint32_t)data[0] << 24) |
        ((uint32_t)data[1] << 16) |
        ((uint32_t)data[2] << 8) |
        (uint32_t)data[3];

    return value;
}

static void writeText(char* target, size_t size, const char* source)
{
    size_t length;

    if (target == NULL || source == NULL || size == 0) {
        return;
    }

    length = strlen(source);

    if (length >= size) {
        length = size - 1;
    }

    memcpy(target, source, length);

    target[length] = '\0';
}

static void setMessage(const char* message)
{
    EnterCriticalSection(&stateLock);

    writeText(
        multiState.message,
        sizeof(multiState.message),
        message
    );

    LeaveCriticalSection(&stateLock);
}

static void setConsoleCursorVisible(int visible)
{
    HANDLE console;
    CONSOLE_CURSOR_INFO cursorInfo;

    console = GetStdHandle(STD_OUTPUT_HANDLE);

    if (console == INVALID_HANDLE_VALUE ||
        console == NULL) {
        return;
    }

    if (!GetConsoleCursorInfo(console, &cursorInfo)) {
        return;
    }

    cursorInfo.bVisible = visible ? TRUE : FALSE;

    SetConsoleCursorInfo(
        console,
        &cursorInfo
    );
}

static void parseRoomState(const uint8_t* data, uint32_t length)
{
    uint32_t offset = 0;
    int i;

    if (length < 12) {
        return;
    }

    EnterCriticalSection(&stateLock);

    memcpy(multiState.roomCode, data, 5);
    multiState.roomCode[5] = '\0';

    offset = 5;

    multiState.mode = data[offset++];
    multiState.phase = data[offset++];
    multiState.modeSelected = data[offset++];

    multiState.restart[0] = data[offset++];
    multiState.restart[1] = data[offset++];

    for (i = 0; i < 2; i++) {
        uint8_t nameLength;

        if (offset + 3 > length) {
            break;
        }

        multiState.present[i] = data[offset++];
        multiState.ready[i] = data[offset++];

        nameLength = data[offset++];

        if (nameLength > NAME_LENGTH ||
            offset + nameLength > length) {
            break;
        }

        memcpy(
            multiState.names[i],
            data + offset,
            nameLength
        );

        multiState.names[i][nameLength] = '\0';

        offset += nameLength;
    }

    if (multiState.screen == SCREEN_WAIT &&
        multiState.phase == 0) {
        multiState.screen = SCREEN_ROOM;
    }

    LeaveCriticalSection(&stateLock);
}

static void parseGameState(const uint8_t* data, uint32_t length)
{
    uint32_t offset = 0;
    int i, y, x;

    if (length < 59) {
        return;
    }

    EnterCriticalSection(&stateLock);

    offset = 4;

    multiState.mode = data[offset++];

    for (i = 0; i < 2; i++) {
        multiState.present[i] = data[offset++];
        multiState.status[i] = data[offset++];

        multiState.score[i] = readU32(data + offset);
        offset += 4;

        multiState.maxExponent[i] = data[offset++];

        multiState.elapsedMs[i] = readU32(data + offset);
        offset += 4;

        for (y = 0; y < 4; y++) {
            for (x = 0; x < 4; x++) {
                multiState.board[i][y][x] = data[offset++];
            }
        }
    }

    if (multiState.screen == SCREEN_WAIT ||
        multiState.screen == SCREEN_ROOM) {
        multiState.screen = SCREEN_GAME;
    }

    LeaveCriticalSection(&stateLock);
}

static DWORD WINAPI receiveThread(LPVOID parameter)
{
    uint8_t type;
    uint8_t data[MAX_PACKET_SIZE];
    uint32_t length;

    (void)parameter;

    while (InterlockedCompareExchange(
        &receiveRunning,
        1,
        1)) {

        if (!ConnectReceive(&type, data, &length)) {
            setMessage("서버와의 연결이 끊어졌습니다.");

            EnterCriticalSection(&stateLock);
            multiState.screen = SCREEN_ERROR;
            LeaveCriticalSection(&stateLock);

            break;
        }

        switch (type) {
        case MSG_ROOM_CREATED:
        case MSG_JOIN_SUCCESS:
            if (length >= 6) {
                EnterCriticalSection(&stateLock);

                memcpy(multiState.roomCode, data, 5);
                multiState.roomCode[5] = '\0';

                multiState.slot = data[5];
                multiState.screen = SCREEN_ROOM;
                multiState.message[0] = '\0';

                LeaveCriticalSection(&stateLock);
            }
            break;

        case MSG_ROOM_STATE:
            parseRoomState(data, length);
            break;

        case MSG_JOIN_FAILED:
        case MSG_ERROR:
            if (length > 0) {
                char message[160];

                uint32_t count = length < sizeof(message) - 1
                    ? length
                    : sizeof(message) - 1;

                memcpy(message, data, count);
                message[count] = '\0';

                setMessage(message);
            }

            EnterCriticalSection(&stateLock);

            if (multiState.screen == SCREEN_WAIT) {
                multiState.screen = SCREEN_MENU;
            }

            LeaveCriticalSection(&stateLock);

            break;

        case MSG_GAME_START:
            EnterCriticalSection(&stateLock);

            multiState.screen = SCREEN_GAME;
            multiState.phase = 1;
            multiState.message[0] = '\0';

            LeaveCriticalSection(&stateLock);
            break;

        case MSG_GAME_STATE:
            parseGameState(data, length);
            break;

        case MSG_GAME_OVER:
            if (length >= 1) {
                char message[80];

                snprintf(
                    message,
                    sizeof(message),
                    "PLAYER %c 게임 오버",
                    data[0] == 0 ? 'A' : 'B'
                );

                setMessage(message);
            }
            break;

        case MSG_GAME_RESULT:
            if (length >= 23) {
                uint32_t offset = 0;
                int i;

                EnterCriticalSection(&stateLock);

                multiState.mode = data[offset++];
                multiState.winner = data[offset++];
                multiState.resultReason = data[offset++];

                for (i = 0; i < 2; i++) {
                    multiState.status[i] = data[offset++];
                    multiState.maxExponent[i] = data[offset++];

                    multiState.score[i] = readU32(data + offset);
                    offset += 4;

                    multiState.elapsedMs[i] = readU32(data + offset);
                    offset += 4;
                }

                multiState.screen = SCREEN_RESULT;

                LeaveCriticalSection(&stateLock);
            }
            break;

        case MSG_RESTART_READY:
            if (length >= 2) {
                EnterCriticalSection(&stateLock);

                multiState.restart[0] = data[0];
                multiState.restart[1] = data[1];

                LeaveCriticalSection(&stateLock);
            }
            break;

        case MSG_PLAYER_LEFT:
            if (length >= 1) {
                uint8_t slot = data[0];
                char message[100];

                if (slot < 2) {
                    EnterCriticalSection(&stateLock);

                    multiState.present[slot] = 0;
                    multiState.status[slot] = PLAYER_LEFT;

                    LeaveCriticalSection(&stateLock);
                }

                snprintf(
                    message,
                    sizeof(message),
                    "PLAYER %c가 방을 나갔습니다. 재시작할 수 없습니다.",
                    slot == 0 ? 'A' : 'B'
                );

                setMessage(message);
            }
            break;

        case MSG_ROOM_CLOSED:
            setMessage("방장이 방을 종료했습니다.");

            EnterCriticalSection(&stateLock);

            multiState.screen = SCREEN_MENU;

            LeaveCriticalSection(&stateLock);
            break;

        default:
            break;
        }
    }

    InterlockedExchange(&receiveRunning, 0);

    return 0;
}

static MultiState getState(void)
{
    MultiState copy;

    EnterCriticalSection(&stateLock);

    copy = multiState;

    LeaveCriticalSection(&stateLock);

    return copy;
}

static uint32_t exponentToTile(uint8_t exponent)
{
    if (exponent == 0) {
        return 0;
    }

    if (exponent >= 31) {
        return 0x80000000u;
    }

    return 1u << exponent;
}

static uint8_t getDigitCount(uint32_t number)
{
    uint8_t count = 0;

    do {
        number /= 10;
        count++;
    } while (number);

    return count;
}

static void getColors(
    uint8_t value,
    uint8_t scheme,
    uint8_t* foreground,
    uint8_t* background)
{
    uint8_t original[] = {
        8, 255, 1, 255, 2, 255, 3, 0,
        4, 255, 5, 255, 6, 255, 7, 0,
        9, 0, 10, 0, 11, 0, 12, 0,
        13, 0, 14, 0, 255, 0, 255, 0
    };

    uint8_t blackwhite[] = {
        232, 255, 234, 255, 236, 255, 238, 255,
        240, 255, 242, 255, 244, 255, 246, 0,
        248, 0, 249, 0, 250, 0, 251, 0,
        252, 0, 253, 0, 254, 0, 255, 0
    };

    uint8_t bluered[] = {
        235, 255, 63, 255, 57, 255, 93, 255,
        129, 255, 165, 255, 201, 255, 200, 255,
        199, 255, 198, 255, 197, 255, 196, 255,
        196, 255, 196, 255, 196, 255, 196, 255
    };

    uint8_t whiteblack[] = {
        255, 0, 254, 0, 253, 0, 252, 0,
        251, 0, 250, 0, 249, 0, 248, 0,
        246, 0, 244, 255, 242, 255, 240, 255,
        238, 255, 236, 255, 234, 255, 232, 255
    };

    uint8_t* schemes[] = {
        original,
        blackwhite,
        bluered,
        whiteblack
    };

    if (scheme > 3) {
        scheme = 0;
    }

    *foreground = *(schemes[scheme] + (1 + value * 2) % sizeof(original));
    *background = *(schemes[scheme] + (0 + value * 2) % sizeof(original));
}

static const char* getModeName(uint8_t mode)
{
    switch (mode) {
    case MODE_SURVIVAL:
        return "3x3 생존전";

    case MODE_2048:
        return "2048 대전";

    case MODE_4096:
        return "4096 대전";

    case MODE_INFINITE:
        return "무한 대전";

    default:
        return "알 수 없음";
    }
}

static void formatTime(
    uint32_t milliseconds,
    char* output,
    size_t size)
{
    uint32_t hours = milliseconds / 3600000;
    uint32_t minutes = (milliseconds / 60000) % 60;
    uint32_t seconds = (milliseconds / 1000) % 60;
    uint32_t ms = milliseconds % 1000;

    snprintf(
        output,
        size,
        "%02u:%02u:%02u.%03u",
        hours,
        minutes,
        seconds,
        ms
    );
}

static void clearScreen(void)
{
    system("cls");
}

static void drawRoom(const MultiState* state)
{
    clearScreen();

    printf("================ MULTIPLAYER WAITING ROOM ================\n\n");

    printf("방 코드: %s\n", state->roomCode);

    printf(
        "게임 모드: %s%s\n\n",
        getModeName(state->mode),
        state->modeSelected ? "" : " (호스트 선택 필요)"
    );

    printf(
        "PLAYER A (HOST): %-16s  연결: %s\n",
        state->names[0][0] ? state->names[0] : "-",
        state->present[0] ? "정상" : "대기"
    );

    printf(
        "PLAYER B:        %-16s  연결: %s  준비: %s\n\n",
        state->names[1][0] ? state->names[1] : "참가 대기",
        state->present[1] ? "정상" : "대기",
        state->ready[1] ? "완료" : "안 함"
    );

    if (state->slot == 0) {
        printf(
            "[1] 3x3 생존전  [2] 2048 대전  [3] 4096 대전  [4] 무한 대전\n"
        );

        if (state->modeSelected &&
            state->present[0] &&
            state->present[1] &&
            state->ready[1]) {

            printf("[S] 게임 시작 (활성)  [Q] 방 나가기\n");
        }
        else {
            printf("[S] 게임 시작 (비활성)  [Q] 방 나가기\n");
        }

        printf(
            "모드 선택, 참가자 접속, 참가자 준비가 모두 완료되어야 시작할 수 있습니다.\n"
        );
    }
    else {
        printf("[R] 준비 / 준비 해제  [Q] 방 나가기\n");

        printf(
            "방장이 게임 모드를 선택하고 시작할 때까지 기다려 주세요.\n"
        );
    }

    if (state->message[0]) {
        printf("\n%s\n", state->message);
    }

    fflush(stdout);
}

static void drawTileCell(uint8_t value, uint8_t scheme, int line)
{
    uint8_t foreground;
    uint8_t background;

    uint32_t number;

    uint8_t digits;
    uint8_t padding;

    getColors(
        value,
        scheme,
        &foreground,
        &background
    );

    printf(
        "\033[38;5;%u;48;5;%um",
        foreground,
        background
    );

    if (line == 0 || line == 2) {
        printf("       ");
    }
    else if (value == 0) {
        printf("   ");
        printWideText(L"\u00B7");
        printf("   ");
    }
    else {
        number = exponentToTile(value);

        digits = getDigitCount(number);

        if (digits >= 7) {
            printf("%7u", number);
        }
        else {
            padding = 7 - digits;

            printf(
                "%*s%u%*s",
                padding - padding / 2,
                "",
                number,
                padding / 2,
                ""
            );
        }
    }

    printf("\033[m");
}

static void drawColoredBoardRow(
    const MultiState* state,
    int y,
    int line,
    int size)
{
    int x;

    for (x = 0; x < size; x++) {
        drawTileCell(
            state->board[0][y][x],
            multiScheme,
            line
        );
    }

    printf("          ");

    for (x = 0; x < size; x++) {
        drawTileCell(
            state->board[1][y][x],
            multiScheme,
            line
        );
    }

    printf("\033[K\n");
}

static void drawGame(const MultiState* state, int clear)
{
    int size = state->mode == MODE_SURVIVAL ? 3 : 4;

    int y;
    int line;

    if (clear) {
        printf("\033[2J");
    }

    printf("\033[H");

    printf("2048.c MULTIPLAYER\033[K\n\n");

    printf(
        "PLAYER A: %-16s                         PLAYER B: %-16s\033[K\n",
        state->names[0][0] ? state->names[0] : "-",
        state->names[1][0] ? state->names[1] : "-"
    );

    printf(
        "%-20u pts  MAX: %-8u              %-20u pts  MAX: %-8u\033[K\n\n",
        state->score[0],
        exponentToTile(state->maxExponent[0]),
        state->score[1],
        exponentToTile(state->maxExponent[1])
    );

    for (y = 0; y < size; y++) {
        for (line = 0; line < 3; line++) {
            drawColoredBoardRow(
                state,
                y,
                line,
                size
            );
        }
    }

    printf("\n");

    printf(
        "내 플레이어: %c\033[K\n",
        state->slot == 0 ? 'A' : 'B'
    );

    printf("방향키 또는 WASD로 이동\033[K\n");

    if (state->status[state->slot] != PLAYER_ACTIVE) {
        printf(
            "현재 내 게임은 종료되었습니다. 상대방이 끝날 때까지 기다리는 중입니다.\033[K\n"
        );
    }

    if (state->status[1 - state->slot] != PLAYER_ACTIVE) {
        printf("상대방 게임 상태: 종료\033[K\n");
    }

    if (state->message[0]) {
        printf("%s\033[K\n", state->message);
    }

    printf("\033[K");
    fflush(stdout);
}

static void drawResult(const MultiState* state)
{
    char timeText[32];
    int i;

    clearScreen();

    printf("====================== GAME RESULT ======================\n\n");

    printf(
        "게임 모드: %s%s\n\n",
        getModeName(state->mode),
        state->modeSelected ? "" : " (호스트 선택 필요)"
    );

    for (i = 0; i < 2; i++) {
        printf(
            "PLAYER %c (%s)\n",
            i == 0 ? 'A' : 'B',
            state->names[i][0] ? state->names[i] : "-"
        );

        printf(
            "최대 타일: %u\n",
            exponentToTile(state->maxExponent[i])
        );

        if (state->mode == MODE_SURVIVAL) {
            formatTime(
                state->elapsedMs[i],
                timeText,
                sizeof(timeText)
            );

            printf("소요 시간: %s\n", timeText);
        }

        if (state->status[i] == PLAYER_WON) {
            printf("상태: 목표 타일 달성\n");
        }
        else if (state->status[i] == PLAYER_LOST) {
            printf("상태: 패배 (상대가 목표 타일 달성)\n");
        }
        else if (state->status[i] == PLAYER_LEFT) {
            printf("상태: 연결 종료\n");
        }
        else if (state->status[i] == PLAYER_OVER &&
            (state->mode == MODE_2048 ||
                state->mode == MODE_4096)) {

            printf("상태: 목표 달성 실패 / 게임 오버\n");
        }
        else if (state->status[i] == PLAYER_OVER) {
            printf("상태: 게임 오버\n");
        }

        printf("\n");
    }

    if (state->winner == 2) {
        printf("RESULT: DRAW\n");
    }
    else {
        printf(
            "WINNER: PLAYER %c\n",
            state->winner == 0 ? 'A' : 'B'
        );
    }

    if (state->present[0] && state->present[1]) {
        printf(
            "\n[R] 재시작 요청  [C] 요청 취소  [M] 메인 화면으로\n"
        );
    }
    else {
        printf(
            "\n[R] 재시작 요청 (비활성)  [M] 메인 화면으로\n"
        );
    }

    if (state->restart[state->slot]) {
        printf(
            "내 재시작 요청 완료. 상대방 응답 대기 중입니다.\n"
        );
    }
    else if (state->restart[1 - state->slot]) {
        printf(
            "상대방이 재시작을 요청했습니다. 동의하려면 R을 누르세요.\n"
        );
    }

    if (!state->present[0] || !state->present[1]) {
        printf("상대방이 퇴장하여 재시작할 수 없습니다.\n");
    }

    if (state->message[0]) {
        printf("%s\n", state->message);
    }

    fflush(stdout);
}

static int readConsoleKey(void)
{
    int key = _getch();

    if (key == 0 || key == 224) {
        return 256 + _getch();
    }

    return key;
}

static void readPlayerName(char* name, size_t size)
{
    printf(
        " 플레이어 이름 (최대 %d자): ",
        NAME_LENGTH
    );

    if (fgets(name, (int)size, stdin) == NULL) {
        writeText(name, size, "PLAYER");
        return;
    }

    name[strcspn(name, "\r\n")] = '\0';

    if (name[0] == '\0') {
        writeText(name, size, "PLAYER");
    }

    if (strlen(name) > NAME_LENGTH) {
        name[NAME_LENGTH] = '\0';
    }
}

static int sendNameRequest(
    uint8_t type,
    const char* name,
    const char* code)
{
    uint8_t data[32];
    uint8_t nameLength = (uint8_t)strlen(name);
    uint32_t offset = 0;

    if (type == MSG_JOIN_ROOM) {
        memcpy(data, code, 5);
        offset = 5;
    }

    data[offset++] = nameLength;

    memcpy(data + offset, name, nameLength);
    offset += nameLength;

    return ConnectSend(type, data, offset);
}

static void sendLeave(void)
{
    ConnectSend(MSG_RETURN_MAIN, NULL, 0);
}

static void processRoomInput(
    const MultiState* state,
    int key)
{
    uint8_t mode;

    if (key == 'q' || key == 'Q' || key == 27) {
        sendLeave();

        EnterCriticalSection(&stateLock);

        multiState.screen = SCREEN_MENU;

        LeaveCriticalSection(&stateLock);

        return;
    }

    if (state->slot == 0) {
        if (key >= '1' && key <= '4') {
            mode = (uint8_t)(key - '1');

            ConnectSend(MSG_MODE_CHANGE, &mode, 1);
        }
        else if (key == 's' || key == 'S') {
            if (state->modeSelected &&
                state->present[0] &&
                state->present[1] &&
                state->ready[1]) {

                ConnectSend(MSG_START_GAME, NULL, 0);
            }
        }
    }
    else if (key == 'r' || key == 'R') {
        if (state->ready[state->slot]) {
            ConnectSend(MSG_UNREADY, NULL, 0);
        }
        else {
            ConnectSend(MSG_READY, NULL, 0);
        }
    }
}

static void processGameInput(int key)
{
    uint8_t direction;
    int valid = 1;

    switch (key) {
    case 256 + 75:
    case 'a':
    case 'A':
        direction = MOVE_LEFT;
        break;

    case 256 + 72:
    case 'w':
    case 'W':
        direction = MOVE_UP;
        break;

    case 256 + 77:
    case 'd':
    case 'D':
        direction = MOVE_RIGHT;
        break;

    case 256 + 80:
    case 's':
    case 'S':
        direction = MOVE_DOWN;
        break;

    default:
        valid = 0;
        direction = 0;
        break;
    }

    if (valid) {
        ConnectSend(MSG_MOVE, &direction, 1);
    }
}

static void processResultInput(
    const MultiState* state,
    int key)
{
    if (key == 'r' || key == 'R') {
        if (state->present[0] && state->present[1]) {
            ConnectSend(MSG_RESTART_REQUEST, NULL, 0);
        }
    }
    else if (key == 'c' || key == 'C') {
        ConnectSend(MSG_RESTART_CANCEL, NULL, 0);
    }
    else if (key == 'm' || key == 'M' || key == 27) {
        sendLeave();

        InterlockedExchange(&receiveRunning, 0);

        return;
    }
}

void startMultiplayer(void)
{
    char name[NAME_LENGTH + 2];
    char code[16];

    int menuResult;
    int exitRequested = 0;

    int hasPreviousState = 0;

    MultiState previousState;

    DWORD threadId;

    memset(&multiState, 0, sizeof(multiState));
    memset(&previousState, 0, sizeof(previousState));

    multiState.screen = SCREEN_MENU;
    multiState.slot = 0;

    InitializeCriticalSection(&stateLock);

    if (!ConnectServer()) {
        clearScreen();

        printf("서버에 연결하지 못했습니다.\n");
        printf("서버가 실행중인지, 서버 IP와 포트 번호가 올바른지 확인하세요.\n");
        printf("IP: %s\n", SERVER_IP);
        printf("PORT: %d\n", SERVER_PORT);

        _getch();

        DeleteCriticalSection(&stateLock);

        return;
    }

    InterlockedExchange(&receiveRunning, 1);

    receiveThreadHandle = CreateThread(
        NULL,
        0,
        receiveThread,
        NULL,
        0,
        &threadId
    );

    if (receiveThreadHandle == NULL) {
        printf("수신 스레드를 생성하지 못했습니다.\n");

        DisconnectServer();

        DeleteCriticalSection(&stateLock);

        return;
    }

    readPlayerName(name, sizeof(name));

    ConnectSend(
        MSG_SET_NAME,
        (const uint8_t*)name,
        (uint32_t)strlen(name)
    );

    setConsoleCursorVisible(0);

    while (!exitRequested) {
        MultiState state = getState();

        int stateChanged;

        stateChanged = !hasPreviousState ||
            memcmp(
                &state,
                &previousState,
                sizeof(MultiState)
            ) != 0;

        if (state.screen == SCREEN_MENU) {
            clearScreen();

            printf("==================== MULTIPLAYER ====================\n\n");

            printf("1. 방 생성\n");
            printf("2. 방 접속\n");
            printf("Q. 메인 화면으로\n\n");

            if (state.message[0]) {
                printf("%s\n\n", state.message);
            }

            printf("선택: ");
            fflush(stdout);

            menuResult = readConsoleKey();

            if (menuResult == '1') {
                EnterCriticalSection(&stateLock);

                multiState.screen = SCREEN_WAIT;
                multiState.message[0] = '\0';

                LeaveCriticalSection(&stateLock);

                ConnectSend(
                    MSG_CREATE_ROOM,
                    (const uint8_t*)name,
                    (uint32_t)strlen(name)
                );
            }
            else if (menuResult == '2') {
                clearScreen();

                printf(
                    "접속할 5자리 방 코드를 입력하세요: "
                );

                setConsoleCursorVisible(1);

                if (fgets(code, sizeof(code), stdin) == NULL) {
                    code[0] = '\0';
                }

                setConsoleCursorVisible(0);

                code[strcspn(code, "\r\n")] = '\0';

                if (strlen(code) != 5) {
                    setMessage(
                        "방 코드는 숫자 5자리로 입력해야 합니다."
                    );
                }
                else {
                    int valid = 1;
                    int i;

                    for (i = 0; i < 5; i++) {
                        if (code[i] < '0' || code[i] > '9') {
                            valid = 0;
                        }
                    }

                    if (!valid) {
                        setMessage(
                            "방 코드는 숫자 5자리로 입력해야 합니다."
                        );
                    }
                    else {
                        EnterCriticalSection(&stateLock);

                        multiState.screen = SCREEN_WAIT;
                        multiState.message[0] = '\0';

                        LeaveCriticalSection(&stateLock);

                        sendNameRequest(
                            MSG_JOIN_ROOM,
                            name,
                            code
                        );
                    }
                }
            }
            else if (menuResult == 'q' ||
                menuResult == 'Q' ||
                menuResult == 27) {

                exitRequested = 1;
            }
        }
        else if (state.screen == SCREEN_WAIT) {
            if (stateChanged) {
                clearScreen();

                printf("서버 응답을 기다리는 중입니다...\n");

                if (state.message[0]) {
                    printf("%s\n", state.message);
                }

                fflush(stdout);
            }

            Sleep(50);
        }
        else if (state.screen == SCREEN_ROOM) {
            if (stateChanged) {
                drawRoom(&state);
            }

            if (_kbhit()) {
                processRoomInput(
                    &state,
                    readConsoleKey()
                );
            }

            Sleep(50);
        }
        else if (state.screen == SCREEN_GAME) {
            if (stateChanged) {
                drawGame(
                    &state,
                    !hasPreviousState ||
                    previousState.screen != SCREEN_GAME
                );
            }

            if (_kbhit()) {
                processGameInput(readConsoleKey());
            }

            Sleep(50);
        }
        else if (state.screen == SCREEN_RESULT) {
            if (stateChanged) {
                drawResult(&state);
            }

            if (_kbhit()) {
                processResultInput(
                    &state,
                    readConsoleKey()
                );

                if (!InterlockedCompareExchange(
                    &receiveRunning,
                    1,
                    1)) {

                    exitRequested = 1;
                }
            }

            Sleep(50);
        }
        else if (state.screen == SCREEN_ERROR) {
            if (stateChanged) {
                clearScreen();

                printf(
                    "멀티플레이 서버와 연결이 끊어졌습니다.\n\n"
                );

                printf("%s\n\n", state.message);

                printf(
                    "아무 키나 누르면 메인 메뉴로 돌아갑니다."
                );

                fflush(stdout);
            }

            _getch();

            exitRequested = 1;
        }

        previousState = state;
        hasPreviousState = 1;
    }

    InterlockedExchange(&receiveRunning, 0);

    ConnectSend(MSG_DISCONNECT, NULL, 0);
    ConnectStopReceive();

    if (receiveThreadHandle != NULL) {
        WaitForSingleObject(
            receiveThreadHandle,
            INFINITE
        );

        CloseHandle(receiveThreadHandle);

        receiveThreadHandle = NULL;
    }

    DisconnectServer();

    setConsoleCursorVisible(1);

    printf("\033[?25h\033[m");

    DeleteCriticalSection(&stateLock);
}