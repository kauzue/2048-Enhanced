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