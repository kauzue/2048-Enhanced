#pragma once

#include <wchar.h>
#include "common.h"

#define UP 1
#define DOWN 2
#define ENTER 3

#define PAUSE_CONTINUE 0
#define PAUSE_SAVE 1
#define PAUSE_RESTART 2
#define PAUSE_RETURN_TO_SINGLE_MENU 3

void getConsoleSize(int* width, int* height);
void MoveCursor(int x, int y);
void printWideText(const wchar_t* text);

int readKey(void);
int ControlKey(void);

int pauseMenu(int mode);

bool saveGame(int mode, uint64_t score, uint8_t board[SIZE][SIZE]);
bool loadGame(int mode, uint64_t* score, uint8_t board[SIZE][SIZE]);

void waitAfterMove(void);
void signal_callback_handler(int signum);