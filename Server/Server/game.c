#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "game.h"
#include "connect.h"

static void getCell(
    const GameState* game,
    uint8_t direction,
    int line,
    int index,
    int* y,
    int* x)
{
    switch (direction) {
    case MOVE_LEFT:
        *y = line;
        *x = index;
        break;

    case MOVE_UP:
        *y = index;
        *x = line;
        break;

    case MOVE_RIGHT:
        *y = line;
        *x = game->size - 1 - index;
        break;

    case MOVE_DOWN:
        *y = game->size - 1 - index;
        *x = line;
        break;

    default:
        *y = 0;
        *x = 0;
        break;
    }
}

void GameAddRandom(GameState* game)
{
    uint8_t emptyY[BOARD_MAX * BOARD_MAX];
    uint8_t emptyX[BOARD_MAX * BOARD_MAX];

    int count = 0;
    int y, x;
    int selected;

    for (y = 0; y < game->size; y++) {
        for (x = 0; x < game->size; x++) {
            if (game->board[y][x] == 0) {
                emptyY[count] = (uint8_t)y;
                emptyX[count] = (uint8_t)x;

                count++;
            }
        }
    }

    if (count == 0) {
        return;
    }

    selected = rand() % count;

    game->board[emptyY[selected]][emptyX[selected]] =
        rand() % 10 == 0 ? 2 : 1;
}

void GameInit(GameState* game, uint8_t size)
{
    memset(game, 0, sizeof(*game));

    game->size = size;
    game->status = GAME_ACTIVE;

    GameAddRandom(game);
    GameAddRandom(game);
}

int GameMove(GameState* game, uint8_t direction)
{
    int line, index;
    int y, x;

    int readIndex;
    int writeIndex;

    int changed = 0;

    uint8_t input[BOARD_MAX];
    uint8_t output[BOARD_MAX];
    uint8_t merged[BOARD_MAX];

    uint64_t score;

    if (direction > MOVE_DOWN ||
        game->status != GAME_ACTIVE ||
        game->size == 0 ||
        game->size > BOARD_MAX) {
        return 0;
    }

    for (line = 0; line < game->size; line++) {
        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));
        memset(merged, 0, sizeof(merged));

        for (index = 0; index < game->size; index++) {
            getCell(
                game,
                direction,
                line,
                index,
                &y,
                &x
            );

            input[index] = game->board[y][x];
        }

        writeIndex = 0;

        for (readIndex = 0; readIndex < game->size; readIndex++) {
            uint8_t value = input[readIndex];

            if (value == 0) {
                continue;
            }

            if (writeIndex > 0 &&
                output[writeIndex - 1] == value &&
                !merged[writeIndex - 1]) {

                output[writeIndex - 1]++;

                merged[writeIndex - 1] = 1;

                if (output[writeIndex - 1] < 32) {
                    score = game->score;

                    score +=
                        (uint64_t)1 << output[writeIndex - 1];

                    game->score = score > UINT32_MAX
                        ? UINT32_MAX
                        : (uint32_t)score;
                }
                else {
                    game->score = UINT32_MAX;
                }
            }
            else {
                output[writeIndex++] = value;
            }
        }

        for (index = 0; index < game->size; index++) {
            getCell(
                game,
                direction,
                line,
                index,
                &y,
                &x
            );

            if (game->board[y][x] != output[index]) {
                changed = 1;
            }

            game->board[y][x] = output[index];
        }
    }

    if (changed) {
        GameAddRandom(game);
    }

    return changed;
}

int GameHasMoves(const GameState* game)
{
    int y, x;

    for (y = 0; y < game->size; y++) {
        for (x = 0; x < game->size; x++) {
            if (game->board[y][x] == 0) {
                return 1;
            }

            if (x + 1 < game->size &&
                game->board[y][x] ==
                game->board[y][x + 1]) {

                return 1;
            }

            if (y + 1 < game->size &&
                game->board[y][x] ==
                game->board[y + 1][x]) {

                return 1;
            }
        }
    }

    return 0;
}

uint8_t GameGetMaxExponent(const GameState* game)
{
    uint8_t maximum = 0;

    int y, x;

    for (y = 0; y < game->size; y++) {
        for (x = 0; x < game->size; x++) {
            if (game->board[y][x] > maximum) {
                maximum = game->board[y][x];
            }
        }
    }

    return maximum;
}