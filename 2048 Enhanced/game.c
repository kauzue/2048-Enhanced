#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "game.h"
#include "system.h"

static uint8_t board[SIZE][SIZE];
static uint8_t scheme = 0;
static uint32_t score = 0;

static void getColors(uint8_t value, uint8_t scheme, uint8_t* foreground, uint8_t* background)
{
    uint8_t original[] = {
        8, 255, 1, 255, 2, 255, 3, 0,
        4, 255, 5, 255, 6, 255, 7, 0,
        9, 0, 10, 0, 11, 0, 12, 0,
        13, 0, 14, 0, 255, 0, 255, 0
    };

    uint8_t blackwhite[] = {
        232, 255, 234, 255, 236, 255, 238, 255,
        240, 255, 242, 255, 244, 255, 246, 0,
        248, 0, 249, 0, 250, 0, 251, 0,
        252, 0, 253, 0, 254, 0, 255, 0
    };

    uint8_t bluered[] = {
        235, 255, 63, 255, 57, 255, 93, 255,
        129, 255, 165, 255, 201, 255, 200, 255,
        199, 255, 198, 255, 197, 255, 196, 255,
        196, 255, 196, 255, 196, 255, 196, 255
    };

    uint8_t whiteblack[] = {
        255, 0, 254, 0, 253, 0, 252, 0,
        251, 0, 250, 0, 249, 0, 248, 0,
        246, 0, 244, 255, 242, 255, 240, 255,
        238, 255, 236, 255, 234, 255, 232, 255
    };

    uint8_t* schemes[] = {
        original,
        blackwhite,
        bluered,
        whiteblack
    };

    *foreground = *(schemes[scheme] + (1 + value * 2) % sizeof(original));
    *background = *(schemes[scheme] + (0 + value * 2) % sizeof(original));
}

static uint8_t getDigitCount(uint32_t number)
{
    uint8_t count = 0;

    do {
        number /= 10;
        count++;
    } while (number);

    return count;
}

static void drawBoard(uint8_t board[SIZE][SIZE], uint8_t scheme, uint32_t score)
{
    uint8_t x, y, fg, bg;

    printf("\033[H");
    printf("2048.c %17u pts\n\n", score);

    for (y = 0; y < SIZE; y++) {
        for (x = 0; x < SIZE; x++) {
            getColors(board[x][y], scheme, &fg, &bg);

            printf("\033[38;5;%u;48;5;%um", fg, bg);
            printf("       ");
            printf("\033[m");
        }

        printf("\n");

        for (x = 0; x < SIZE; x++) {
            getColors(board[x][y], scheme, &fg, &bg);

            printf("\033[38;5;%u;48;5;%um", fg, bg);

            if (board[x][y] != 0) {
                uint32_t number = 1 << board[x][y];
                uint8_t t = 7 - getDigitCount(number);

                printf("%*s%u%*s", t - t / 2, "", number, t / 2, "");
            }
            else {
                printf("   ");
                printWideText(L"\u00B7");
                printf("   ");
            }

            printf("\033[m");
        }

        printf("\n");

        for (x = 0; x < SIZE; x++) {
            getColors(board[x][y], scheme, &fg, &bg);

            printf("\033[38;5;%u;48;5;%um", fg, bg);
            printf("       ");
            printf("\033[m");
        }

        printf("\n");
    }

    printf("\n        ");
    printWideText(L"\u2190,\u2191,\u2192,\u2193");
    printf(" or q        \n");

    printf("\033[A");
    fflush(stdout);
}

static uint8_t findTarget(uint8_t array[SIZE], uint8_t x, uint8_t stop)
{
    uint8_t t;

    if (x == 0) {
        return x;
    }

    for (t = x - 1;; t--) {
        if (array[t] != 0) {
            if (array[t] != array[x]) {
                return t + 1;
            }

            return t;
        }
        else if (t == stop) {
            return t;
        }
    }

    return x;
}

static bool slideArray(uint8_t array[SIZE], uint32_t* score)
{
    bool success = false;
    uint8_t x, t, stop = 0;

    for (x = 0; x < SIZE; x++) {
        if (array[x] != 0) {
            t = findTarget(array, x, stop);

            if (t != x) {
                if (array[t] == 0) {
                    array[t] = array[x];
                }
                else if (array[t] == array[x]) {
                    array[t]++;
                    *score += 1 << array[t];
                    stop = t + 1;
                }

                array[x] = 0;
                success = true;
            }
        }
    }

    return success;
}

static void rotateBoard(uint8_t board[SIZE][SIZE])
{
    uint8_t i, j, n = SIZE;
    uint8_t tmp;

    for (i = 0; i < n / 2; i++) {
        for (j = i; j < n - i - 1; j++) {
            tmp = board[i][j];
            board[i][j] = board[j][n - i - 1];
            board[j][n - i - 1] = board[n - i - 1][n - j - 1];
            board[n - i - 1][n - j - 1] = board[n - j - 1][i];
            board[n - j - 1][i] = tmp;
        }
    }
}

static bool moveUp(uint8_t board[SIZE][SIZE], uint32_t* score)
{
    bool success = false;
    uint8_t x;

    for (x = 0; x < SIZE; x++) {
        success |= slideArray(board[x], score);
    }

    return success;
}

static bool moveLeft(uint8_t board[SIZE][SIZE], uint32_t* score)
{
    bool success;

    rotateBoard(board);
    success = moveUp(board, score);

    rotateBoard(board);
    rotateBoard(board);
    rotateBoard(board);

    return success;
}

static bool moveDown(uint8_t board[SIZE][SIZE], uint32_t* score)
{
    bool success;

    rotateBoard(board);
    rotateBoard(board);

    success = moveUp(board, score);

    rotateBoard(board);
    rotateBoard(board);

    return success;
}

static bool moveRight(uint8_t board[SIZE][SIZE], uint32_t* score)
{
    bool success;

    rotateBoard(board);
    rotateBoard(board);
    rotateBoard(board);

    success = moveUp(board, score);

    rotateBoard(board);

    return success;
}

static bool findPairDown(uint8_t board[SIZE][SIZE])
{
    uint8_t x, y;

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE - 1; y++) {
            if (board[x][y] == board[x][y + 1]) {
                return true;
            }
        }
    }

    return false;
}

static uint8_t countEmpty(uint8_t board[SIZE][SIZE])
{
    uint8_t x, y;
    uint8_t count = 0;

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            if (board[x][y] == 0) {
                count++;
            }
        }
    }

    return count;
}

static bool gameEnded(uint8_t board[SIZE][SIZE])
{
    bool ended = true;

    if (countEmpty(board) > 0) {
        return false;
    }

    if (findPairDown(board)) {
        return false;
    }

    rotateBoard(board);

    if (findPairDown(board)) {
        ended = false;
    }

    rotateBoard(board);
    rotateBoard(board);
    rotateBoard(board);

    return ended;
}

static void addRandom(uint8_t board[SIZE][SIZE])
{
    static bool initialized = false;

    uint8_t x, y;
    uint8_t r, len = 0;
    uint8_t n, list[SIZE * SIZE][2];

    if (!initialized) {
        srand((unsigned int)time(NULL));
        initialized = true;
    }

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            if (board[x][y] == 0) {
                list[len][0] = x;
                list[len][1] = y;
                len++;
            }
        }
    }

    if (len > 0) {
        r = rand() % len;
        x = list[r][0];
        y = list[r][1];
        n = (rand() % 10) / 9 + 1;

        board[x][y] = n;
    }
}

static void initBoard(uint8_t board[SIZE][SIZE])
{
    uint8_t x, y;

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            board[x][y] = 0;
        }
    }

    addRandom(board);
    addRandom(board);
}

void startGame(void)
{
    int c;
    bool success;

    printf("\033[?25l\033[2J");

    initBoard(board);
    drawBoard(board, scheme, score);

    while (true) {
        c = readKey();

        switch (c) {
        case 52:
        case 97:
        case 104:
        case 68:
            success = moveLeft(board, &score);
            break;

        case 54:
        case 100:
        case 108:
        case 67:
            success = moveRight(board, &score);
            break;

        case 56:
        case 119:
        case 107:
        case 65:
            success = moveUp(board, &score);
            break;

        case 50:
        case 115:
        case 106:
        case 66:
            success = moveDown(board, &score);
            break;

        default:
            success = false;
            break;
        }

        if (success) {
            drawBoard(board, scheme, score);
            waitAfterMove();

            addRandom(board);
            drawBoard(board, scheme, score);

            if (gameEnded(board)) {
                printf("         GAME OVER          \n");
                break;
            }
        }

        if (c == 'q') {
            printf("        QUIT? (y/n)         \n");

            c = readKey();

            if (c == 'y') {
                break;
            }

            drawBoard(board, scheme, score);
        }

        if (c == 'r') {
            printf("       RESTART? (y/n)       \n");

            c = readKey();

            if (c == 'y') {
                initBoard(board);
                score = 0;
            }

            drawBoard(board, scheme, score);
        }
    }

    printf("\033[?25h\033[m");
}