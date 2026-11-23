#pragma once

#include <stdint.h>

#define BOARD_MAX 4

#define GAME_ACTIVE 0
#define GAME_OVER 1
#define GAME_WON 2
#define GAME_LOST 3
#define GAME_LEFT 4

typedef struct {
    uint8_t board[BOARD_MAX][BOARD_MAX];

    uint8_t size;
    uint8_t status;

    uint32_t score;
    uint32_t elapsedMs;
} GameState;

void GameInit(GameState* game, uint8_t size);

int GameMove(GameState* game, uint8_t direction);

int GameHasMoves(const GameState* game);

uint8_t GameGetMaxExponent(const GameState* game);

void GameAddRandom(GameState* game);