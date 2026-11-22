#pragma once

#include <wchar.h>

#define UP 1
#define DOWN 2
#define ENTER 3

void MoveCursor(int x, int y);
void printWideText(const wchar_t* text);

int readKey(void);
int ControlKey(void);

void waitAfterMove(void);
void signal_callback_handler(int signum);