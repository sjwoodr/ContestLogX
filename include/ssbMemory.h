/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef SSBMEMORY_H
#define SSBMEMORY_H

#include <QString>
#include "memoryRole.h"

struct SsbMemory {
    QString abbreviation;  // 6 chars max for button display
    QString text;          // Full SSB text to speak via TTS
    MemoryRole role = MemoryRole::NoRole;  // Optional Run/S&P role
};

#endif // SSBMEMORY_H
