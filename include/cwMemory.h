/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef CWMEMORY_H
#define CWMEMORY_H

#include <QString>

struct CwMemory {
    QString abbreviation;  // 5 chars max for button display
    QString text;          // Full CW text to send
};

#endif // CWMEMORY_H
