#pragma once

#include <stdint.h>

#include "connect.h"

void RoomInit(void);

void RoomHandleMessage(
    Client* client,
    uint8_t type,
    const uint8_t* data,
    uint32_t length
);

void RoomDisconnect(Client* client);