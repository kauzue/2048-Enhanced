#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "single.h"
#include "system.h"

static uint8_t board[SIZE][SIZE];
static uint8_t scheme = 0;
static uint64_t score = 0;
static int currentMode = SINGLE_MODE_2048;

static const char* getModeName(void)
{
    switch (currentMode) {
    case SINGLE_MODE_2048:
        return "2048";

    case SINGLE_MODE_4096:
        return "4096";

    case SINGLE_MODE_INFINITE:
        return "Infinite";

    default:
        return "2048";
    }
}

static uint8_t getTarget(void)
{
    switch (currentMode) {
    case SINGLE_MODE_2048:
        return 11;

    case SINGLE_MODE_4096:
        return 12;

    default:
        return 0;
    }
}

static uint64_t getTileValue(uint8_t exponent)
{
    if (exponent >= 64) {
        return UINT64_MAX;
    }

    return (uint64_t)1 << exponent;
}

static void getColors(
    uint8_t value,
    uint8_t* foreground,
    uint8_t* background
)
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

    if (scheme > 3) {
        scheme = 0;
    }

    *foreground =
        *(schemes[scheme] + (1 + value * 2) % sizeof(original));

    *background =
        *(schemes[scheme] + (0 + value * 2) % sizeof(original));
}

static uint8_t getDigitCount(uint64_t number)
{
    uint8_t count = 0;

    do {
        number /= 10;
        count++;
    } while (number);

    return count;
}

static void drawBoard(void)
{
    uint8_t x, y, fg, bg;

    printf("\033[H");

    printf(
        "2048 Enhanced - %s %17llu pts\n\n",
        getModeName(),
        (unsigned long long)score
    );

    for (y = 0; y < SIZE; y++) {
        for (x = 0; x < SIZE; x++) {
            getColors(board[x][y], &fg, &bg);

            printf(
                "\033[38;5;%u;48;5;%um",
                (unsigned int)fg,
                (unsigned int)bg
            );

            printf("       ");
            printf("\033[m");
        }

        printf("\n");

        for (x = 0; x < SIZE; x++) {
            getColors(board[x][y], &fg, &bg);

            printf(
                "\033[38;5;%u;48;5;%um",
                (unsigned int)fg,
                (unsigned int)bg
            );

            if (board[x][y] != 0) {
                uint64_t number = getTileValue(board[x][y]);
                uint8_t digits = getDigitCount(number);
                int padding = 7 - digits;
                int left = 0;
                int right = 0;

                if (padding > 0) {
                    left = padding - padding / 2;
                    right = padding / 2;
                }

                printf(
                    "%*s%llu%*s",
                    left,
                    "",
                    (unsigned long long)number,
                    right,
                    ""
                );
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
            getColors(board[x][y], &fg, &bg);

            printf(
                "\033[38;5;%u;48;5;%um",
                (unsigned int)fg,
                (unsigned int)bg
            );

            printf("       ");
            printf("\033[m");
        }

        printf("\n");
    }

    printf("\n        ");

    printWideText(L"\u2190,\u2191,\u2192,\u2193");

    printf(" or ESC        \n");

    fflush(stdout);
}

static uint8_t findTarget(
    uint8_t array[SIZE],
    uint8_t x,
    uint8_t stop
)
{
    int t;

    if (x == 0) {
        return x;
    }

    for (t = (int)x - 1; t >= (int)stop; t--) {
        if (array[t] != 0) {
            if (array[t] != array[x]) {
                return (uint8_t)(t + 1);
            }

            return (uint8_t)t;
        }

        if (t == (int)stop) {
            return (uint8_t)t;
        }
    }

    return x;
}

static bool slideArray(uint8_t array[SIZE])
{
    bool success = false;
    uint8_t x, t, stop = 0;
    uint64_t tileScore;

    for (x = 0; x < SIZE; x++) {
        if (array[x] != 0) {
            t = findTarget(array, x, stop);

            if (t != x) {
                if (array[t] == 0) {
                    array[t] = array[x];
                }
                else if (
                    array[t] == array[x] &&
                    array[t] < 63
                    ) {
                    array[t]++;

                    tileScore = getTileValue(array[t]);

                    if (UINT64_MAX - score < tileScore) {
                        score = UINT64_MAX;
                    }
                    else {
                        score += tileScore;
                    }

                    stop = t + 1;
                }
                else {
                    continue;
                }

                array[x] = 0;
                success = true;
            }
        }
    }

    return success;
}

static void rotateBoard(void)
{
    uint8_t i, j, n = SIZE;
    uint8_t tmp;

    for (i = 0; i < n / 2; i++) {
        for (j = i; j < n - i - 1; j++) {
            tmp = board[i][j];

            board[i][j] = board[j][n - i - 1];

            board[j][n - i - 1] =
                board[n - i - 1][n - j - 1];

            board[n - i - 1][n - j - 1] =
                board[n - j - 1][i];

            board[n - j - 1][i] = tmp;
        }
    }
}

static bool moveUp(void)
{
    bool success = false;
    uint8_t x;

    for (x = 0; x < SIZE; x++) {
        success |= slideArray(board[x]);
    }

    return success;
}

static bool moveLeft(void)
{
    bool success;

    rotateBoard();

    success = moveUp();

    rotateBoard();
    rotateBoard();
    rotateBoard();

    return success;
}

static bool moveDown(void)
{
    bool success;

    rotateBoard();
    rotateBoard();

    success = moveUp();

    rotateBoard();
    rotateBoard();

    return success;
}

static bool moveRight(void)
{
    bool success;

    rotateBoard();
    rotateBoard();
    rotateBoard();

    success = moveUp();

    rotateBoard();

    return success;
}

static bool findPairDown(void)
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

static uint8_t countEmpty(void)
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

static bool gameEnded(void)
{
    bool ended = true;

    if (countEmpty() > 0) {
        return false;
    }

    if (findPairDown()) {
        return false;
    }

    rotateBoard();

    if (findPairDown()) {
        ended = false;
    }

    rotateBoard();
    rotateBoard();
    rotateBoard();

    return ended;
}

static void addRandom(void)
{
    static bool initialized = false;

    uint8_t x, y;
    uint8_t r, len = 0;
    uint8_t n;
    uint8_t list[SIZE * SIZE][2];

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

static void initBoard(void)
{
    uint8_t x, y;

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            board[x][y] = 0;
        }
    }

    addRandom();
    addRandom();
}

static bool hasTargetTile(void)
{
    uint8_t target = getTarget();
    uint8_t x, y;

    if (target == 0) {
        return false;
    }

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            if (board[x][y] >= target) {
                return true;
            }
        }
    }

    return false;
}

static void showSaveResult(bool success)
{
    system("cls");

    if (success) {
        printWideText(L"현재 모드의 게임을 저장했습니다.\n\n");
    }
    else {
        printWideText(L"저장에 실패했습니다.\n\n");
    }

    printWideText(
        L"아무 키나 누르면 게임으로 돌아갑니다."
    );

    readKey();
}

static void showGameResult(bool won)
{
    printf("\n");

    if (won) {
        printf("           YOU WIN!          \n");
    }
    else {
        printf("          GAME OVER          \n");
    }

    printWideText(
        L"\n아무 키나 누르면 돌아갑니다."
    );

    readKey();
}

static void runSingleGame(void)
{
    int key;
    int action;

    bool success;
    bool won = false;
    bool ended = false;
    bool backToMenu = false;

    printf("\033[?25l\033[2J\033[H");

    drawBoard();

    if (hasTargetTile()) {
        won = true;
    }
    else if (gameEnded()) {
        ended = true;
    }

    while (!won && !ended) {
        key = readKey();

        if (key == 27) {
            action = pauseMenu(currentMode);

            if (action == PAUSE_SAVE) {
                showSaveResult(
                    saveGame(currentMode, score, board)
                );

                printf("\033[2J\033[H");
                drawBoard();

                continue;
            }

            if (action == PAUSE_RESTART) {
                score = 0;
                initBoard();

                printf("\033[2J\033[H");
                drawBoard();

                continue;
            }

            if (action == PAUSE_RETURN_TO_SINGLE_MENU) {
                backToMenu = true;
                break;
            }

            printf("\033[2J\033[H");
            drawBoard();

            continue;
        }

        switch (key) {
        case 52:
        case 97:
        case 104:
        case 68:
            success = moveLeft();
            break;

        case 54:
        case 100:
        case 108:
        case 67:
            success = moveRight();
            break;

        case 56:
        case 119:
        case 107:
        case 65:
            success = moveUp();
            break;

        case 50:
        case 115:
        case 106:
        case 66:
            success = moveDown();
            break;

        default:
            success = false;
            break;
        }

        if (success) {
            drawBoard();

            waitAfterMove();

            if (hasTargetTile()) {
                won = true;
                break;
            }

            addRandom();

            drawBoard();

            if (gameEnded()) {
                ended = true;
            }
        }
    }

    if (!backToMenu) {
        showGameResult(won);
    }

    printf("\033[?25h\033[m");
}

void startSingleGame(int mode)
{
    if (
        mode < SINGLE_MODE_2048 ||
        mode > SINGLE_MODE_INFINITE
        ) {
        return;
    }

    currentMode = mode;
    score = 0;

    initBoard();

    runSingleGame();
}

bool loadSingleGame(int mode)
{
    uint64_t loadedScore;
    uint8_t loadedBoard[SIZE][SIZE];

    int x, y;

    if (
        mode < SINGLE_MODE_2048 ||
        mode > SINGLE_MODE_INFINITE
        ) {
        return false;
    }

    if (!loadGame(mode, &loadedScore, loadedBoard)) {
        return false;
    }

    currentMode = mode;
    score = loadedScore;

    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            board[x][y] = loadedBoard[x][y];
        }
    }

    runSingleGame();

    return true;
}