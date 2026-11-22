#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <windows.h>

#include "init.h"
#include "game.h"
#include "system.h"

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

void init(void)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (GetConsoleMode(output, &mode)) {
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    signal(SIGINT, signal_callback_handler);
}

void printMainMenu(void)
{
    const wchar_t* menuItems[] = {
        L"싱글 플레이",
        L"멀티 플레이",
        L"설정",
        L"종료"
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

        x = (width - 13) / 2 + 2;
        titleX = (width - 13) / 2;
        y = (height - 9) / 2;

        if (x < 2) x = 2;
        if (titleX < 0) titleX = 0;
        if (y < 0) y = 0;

        MoveCursor(titleX, y);
        printWideText(L"2048 Enhanced");

        for (i = 0; i < 4; i++) {
            MoveCursor(x, y + 2 + i * 2);
            printWideText(menuItems[i]);
        }

        MoveCursor(x - 2, y + 2 + selected * 2);
        printf(">");

        fflush(stdout);

        key = 0;

        while (key != ENTER) {
            key = ControlKey();

            switch (key) {
            case UP:
                if (selected > 0) {
                    MoveCursor(x - 2, y + 2 + selected * 2);
                    printf(" ");

                    selected--;

                    MoveCursor(x - 2, y + 2 + selected * 2);
                    printf(">");
                }
                break;

            case DOWN:
                if (selected < 3) {
                    MoveCursor(x - 2, y + 2 + selected * 2);
                    printf(" ");

                    selected++;

                    MoveCursor(x - 2, y + 2 + selected * 2);
                    printf(">");
                }
                break;
            }

            fflush(stdout);
        }

        switch (selected) {
        case 0:
            startGame();
            break;

        case 1:
            system("cls");
            printWideText(L"멀티 플레이는 아직 준비 중입니다.\n");
            printWideText(L"아무 키나 누르면 돌아갑니다.");
            readKey();
            break;

        case 2:
            system("cls");
            printWideText(L"설정 메뉴는 아직 준비 중입니다.\n");
            printWideText(L"아무 키나 누르면 돌아갑니다.");
            readKey();
            break;

        case 3:
            system("cls");
            return;
        }
    }
}