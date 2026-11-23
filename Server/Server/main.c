#include "connect.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    serverRun();

    return EXIT_SUCCESS;
}