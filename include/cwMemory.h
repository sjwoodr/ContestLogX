/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CWMEMORY_H
#define CWMEMORY_H

#include <QString>
#include "memoryRole.h"

struct CwMemory {
    QString abbreviation;  // 5 chars max for button display
    QString text;          // Full CW text to send
    MemoryRole role = MemoryRole::NoRole;  // Optional Run/S&P role
};

#endif // CWMEMORY_H
