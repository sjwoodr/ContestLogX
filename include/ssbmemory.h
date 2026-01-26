/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef SSBMEMORY_H
#define SSBMEMORY_H

#include <QString>

struct SsbMemory {
    QString abbreviation;  // 5 chars max for button display
    QString text;          // Full SSB text to speak via TTS
};

#endif // SSBMEMORY_H
