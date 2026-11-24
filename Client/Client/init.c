#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <windows.h>

#include "init.h"
#include "single.h"
#include "system.h"
#include "multy.h"

static void showMessage(const wchar_t* message)
{
    system("cls");

    printWideText(message);
    printWideText(L"\n\n아무 키나 누르면 돌아갑니다.");

    readKey();
}

static void printLoadMenu(void)
{
    const wchar_t* menuItems[] = {
        L"2048 모드 불러오기",
        L"4096 모드 불러오기",
        L"무한 모드 불러오기",
        L"뒤로 가기"
    };

    const int loadModes[] = {
        SINGLE_MODE_2048,
        SINGLE_MODE_4096,
        SINGLE_MODE_INFINITE
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

        x = (width - 28) / 2 + 2;
        titleX = (width - 9) / 2;
        y = (height - 9) / 2;

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
        printWideText(L"불러오기");

        for (i = 0; i < 4; i++) {
            MoveCursor(x, y + 2 + i * 2);
            printWideText(menuItems[i]);
        }

        MoveCursor(x - 2, y + 2 + selected * 2);
        printf(">");

        fflush(stdout);

        while (1) {
            key = ControlKey();

            if (key == ENTER) {
                break;
            }

            switch (key) {
            case UP:
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
                break;

            case DOWN:
                if (selected < 3) {
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
                break;
            }

            fflush(stdout);
        }

        if (selected == 3) {
            return;
        }

        if (!loadSingleGame(loadModes[selected])) {
            showMessage(
                L"해당 모드의 저장 파일이 없거나 불러올 수 없습니다."
            );
        }
    }
}

static void printSingleMenu(void)
{
    const wchar_t* menuItems[] = {
        L"2048 모드",
        L"4096 모드",
        L"무한 모드",
        L"불러오기",
        L"최고 기록",
        L"뒤로 가기"
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

        x = (width - 20) / 2 + 2;
        titleX = (width - 15) / 2;
        y = (height - 13) / 2;

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
        printWideText(L"싱글 플레이");

        for (i = 0; i < 6; i++) {
            MoveCursor(x, y + 2 + i * 2);
            printWideText(menuItems[i]);
        }

        MoveCursor(x - 2, y + 2 + selected * 2);
        printf(">");

        fflush(stdout);

        while (1) {
            key = ControlKey();

            if (key == ENTER) {
                break;
            }

            switch (key) {
            case UP:
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
                break;

            case DOWN:
                if (selected < 5) {
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
                break;
            }

            fflush(stdout);
        }

        switch (selected) {
        case 0:
            startSingleGame(SINGLE_MODE_2048);
            break;

        case 1:
            startSingleGame(SINGLE_MODE_4096);
            break;

        case 2:
            startSingleGame(SINGLE_MODE_INFINITE);
            break;

        case 3:
            printLoadMenu();
            break;

        case 4:
            showMessage(
                L"최고 기록 기능은 아직 준비 중입니다."
            );
            break;

        case 5:
            return;
        }
    }
}

void init(void)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (GetConsoleMode(output, &mode)) {
        SetConsoleMode(
            output,
            mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING
        );
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
        printWideText(L"2048 Enhanced");

        for (i = 0; i < 4; i++) {
            MoveCursor(x, y + 2 + i * 2);
            printWideText(menuItems[i]);
        }

        MoveCursor(x - 2, y + 2 + selected * 2);
        printf(">");

        fflush(stdout);

        while (1) {
            key = ControlKey();

            if (key == ENTER) {
                break;
            }

            switch (key) {
            case UP:
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
                break;

            case DOWN:
                if (selected < 3) {
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
                break;
            }

            fflush(stdout);
        }

        switch (selected) {
        case 0:
            printSingleMenu();
            break;

        case 1:
            startMultiplayer();
            break;

        case 2:
            showMessage(
                L"설정 메뉴는 아직 준비 중입니다."
            );
            break;

        case 3:
            system("cls");
            return;
        }
    }
}