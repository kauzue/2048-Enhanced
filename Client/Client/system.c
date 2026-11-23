#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <conio.h>
#include <wchar.h>

#include "system.h"

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

static void getConsoleSize(int* width, int* height)
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

int pauseMenu(void)
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
            return PAUSE_SAVE;

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