#pragma once

#include <wchar.h>

#define UP 1
#define DOWN 2
#define ENTER 3

#define PAUSE_CONTINUE 0
#define PAUSE_SAVE 1
#define PAUSE_RESTART 2
#define PAUSE_RETURN_TO_SINGLE_MENU 3

void MoveCursor(int x, int y);
void printWideText(const wchar_t* text);

int readKey(void);
int ControlKey(void);

int pauseMenu(void);

void waitAfterMove(void);
void signal_callback_handler(int signum);