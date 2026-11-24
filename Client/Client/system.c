#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>
#include <wchar.h>

#include "system.h"

#define SAVE_VERSION 1

#define SAVE_HEADER \
    "version,mode,score," \
    "b00,b01,b02,b03," \
    "b10,b11,b12,b13," \
    "b20,b21,b22,b23," \
    "b30,b31,b32,b33"

void MoveCursor(int x, int y)
{
    HANDLE consolehandle = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos;

    pos.X = (SHORT)x;
    pos.Y = (SHORT)y;

    SetConsoleCursorPosition(consolehandle, pos);
}

void printWideText(const wchar_t* text)
{
    HANDLE consolehandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;

    fflush(stdout);

    WriteConsoleW(
        consolehandle,
        text,
        (DWORD)wcslen(text),
        &written,
        NULL
    );
}

static int getKeyCode(void)
{
    int key = _getch();

    if (key == 0 || key == 224) {
        return 256 + _getch();
    }

    return key;
}

int readKey(void)
{
    int key = getKeyCode();

    switch (key) {
    case 256 + 75: return 68;
    case 256 + 77: return 67;
    case 256 + 72: return 65;
    case 256 + 80: return 66;
    default: return key;
    }
}

int ControlKey(void)
{
    switch (readKey()) {
    case 65: return UP;
    case 66: return DOWN;
    case '\r': return ENTER;
    default: return 0;
    }
}

static const char* getSaveFileName(int mode)
{
    switch (mode) {
    case 0:
        return "save_2048.csv";

    case 1:
        return "save_4096.csv";

    case 2:
        return "save_infinite.csv";

    default:
        return NULL;
    }
}

static const wchar_t* getModeName(int mode)
{
    switch (mode) {
    case 0:
        return L"2048";

    case 1:
        return L"4096";

    case 2:
        return L"무한";

    default:
        return L"알 수 없음";
    }
}

void getConsoleSize(int* width, int* height)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE consolehandle = GetStdHandle(STD_OUTPUT_HANDLE);

    *width = 80;
    *height = 25;

    if (GetConsoleScreenBufferInfo(consolehandle, &info)) {
        *width = info.srWindow.Right - info.srWindow.Left + 1;
        *height = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
}

static void showPauseSettings(void)
{
    system("cls");

    printWideText(
        L"설정 메뉴는 아직 준비 중입니다.\n\n"
    );

    printWideText(
        L"아무 키나 누르면 돌아갑니다."
    );

    readKey();
}

static bool confirmSave(int mode)
{
    int key;
    const char* fileName = getSaveFileName(mode);

    system("cls");

    printWideText(L"저장 확인\n\n");

    printWideText(L"");

    printWideText(getModeName(mode));
    printWideText(L" 모드의 저장 파일을 덮어쓸 수 있습니다.\n\n");

    printWideText(
        L"저장하시겠습니까? (Y/N)"
    );

    while (1) {
        key = readKey();

        switch (key) {
        case 'y':
        case 'Y':
            return true;

        case 'n':
        case 'N':
        case 27:
            return false;
        }
    }
}

int pauseMenu(int mode)
{
    const wchar_t* menuItems[] = {
        L"계속하기",
        L"저장하기",
        L"설정",
        L"다시 시작",
        L"싱글 플레이 메뉴로 돌아가기"
    };

    int width, height;
    int x, y;
    int titleX;
    int key;
    int selected = 0;
    int i;

    while (1) {
        system("cls");

        getConsoleSize(&width, &height);

        x = (width - 25) / 2 + 2;
        titleX = (width - 10) / 2;
        y = (height - 11) / 2;

        if (x < 2) {
            x = 2;
        }

        if (titleX < 0) {
            titleX = 0;
        }

        if (y < 0) {
            y = 0;
        }

        MoveCursor(titleX, y);
        printWideText(L"일시정지");

        for (i = 0; i < 5; i++) {
            MoveCursor(x, y + 2 + i * 2);
            printWideText(menuItems[i]);
        }

        MoveCursor(x - 2, y + 2 + selected * 2);
        printf(">");

        fflush(stdout);

        while (1) {
            key = readKey();

            if (key == 27) {
                return PAUSE_CONTINUE;
            }

            if (key == 65) {
                if (selected > 0) {
                    MoveCursor(
                        x - 2,
                        y + 2 + selected * 2
                    );

                    printf(" ");

                    selected--;

                    MoveCursor(
                        x - 2,
                        y + 2 + selected * 2
                    );

                    printf(">");
                }
            }
            else if (key == 66) {
                if (selected < 4) {
                    MoveCursor(
                        x - 2,
                        y + 2 + selected * 2
                    );

                    printf(" ");

                    selected++;

                    MoveCursor(
                        x - 2,
                        y + 2 + selected * 2
                    );

                    printf(">");
                }
            }
            else if (key == '\r') {
                break;
            }

            fflush(stdout);
        }

        switch (selected) {
        case 0:
            return PAUSE_CONTINUE;

        case 1:
            if (confirmSave(mode)) {
                return PAUSE_SAVE;
            }
            break;

        case 2:
            showPauseSettings();
            break;

        case 3:
            return PAUSE_RESTART;

        case 4:
            return PAUSE_RETURN_TO_SINGLE_MENU;
        }
    }
}

bool saveGame(
    int mode,
    uint64_t score,
    uint8_t board[SIZE][SIZE]
)
{
    FILE* file = NULL;
    const char* fileName;
    int x, y;
    bool success = true;

    fileName = getSaveFileName(mode);

    if (fileName == NULL) {
        return false;
    }

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            if (board[x][y] > 63) {
                return false;
            }
        }
    }

    if (fopen_s(&file, fileName, "w") != 0 || file == NULL) {
        return false;
    }

    if (fprintf(file, SAVE_HEADER "\n") < 0) {
        success = false;
    }

    if (
        success &&
        fprintf(
            file,
            "%d,%d,%llu",
            SAVE_VERSION,
            mode,
            (unsigned long long)score
        ) < 0
        ) {
        success = false;
    }

    for (x = 0; x < SIZE && success; x++) {
        for (y = 0; y < SIZE; y++) {
            if (fprintf(file, ",%u", board[x][y]) < 0) {
                success = false;
                break;
            }
        }
    }

    if (success && fprintf(file, "\n") < 0) {
        success = false;
    }

    if (fclose(file) != 0) {
        success = false;
    }

    return success;
}

bool loadGame(
    int mode,
    uint64_t* score,
    uint8_t board[SIZE][SIZE]
)
{
    FILE* file = NULL;
    const char* fileName;

    char header[256];
    char line[512];

    unsigned int version;
    unsigned int loadedMode;
    unsigned int tiles[SIZE][SIZE];

    unsigned long long loadedScore;

    int fields;
    int x, y;

    fileName = getSaveFileName(mode);

    if (
        fileName == NULL ||
        score == NULL ||
        board == NULL
        ) {
        return false;
    }

    if (fopen_s(&file, fileName, "r") != 0 || file == NULL) {
        return false;
    }

    if (fgets(header, sizeof(header), file) == NULL) {
        fclose(file);
        return false;
    }

    header[strcspn(header, "\r\n")] = '\0';

    if (strcmp(header, SAVE_HEADER) != 0) {
        fclose(file);
        return false;
    }

    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return false;
    }

    fields = sscanf_s(
        line,
        "%u,%u,%llu,"
        "%u,%u,%u,%u,"
        "%u,%u,%u,%u,"
        "%u,%u,%u,%u,"
        "%u,%u,%u,%u",
        &version,
        &loadedMode,
        &loadedScore,
        &tiles[0][0],
        &tiles[0][1],
        &tiles[0][2],
        &tiles[0][3],
        &tiles[1][0],
        &tiles[1][1],
        &tiles[1][2],
        &tiles[1][3],
        &tiles[2][0],
        &tiles[2][1],
        &tiles[2][2],
        &tiles[2][3],
        &tiles[3][0],
        &tiles[3][1],
        &tiles[3][2],
        &tiles[3][3]
    );

    fclose(file);

    if (fields != 19) {
        return false;
    }

    if (
        version != SAVE_VERSION ||
        loadedMode != (unsigned int)mode
        ) {
        return false;
    }

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            if (tiles[x][y] > 63) {
                return false;
            }
        }
    }

    *score = (uint64_t)loadedScore;

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            board[x][y] = (uint8_t)tiles[x][y];
        }
    }

    return true;
}

void waitAfterMove(void)
{
    Sleep(150);
}

void signal_callback_handler(int signum)
{
    printf("         TERMINATED         \n");
    printf("\033[?25h\033[m");

    exit(signum);
}