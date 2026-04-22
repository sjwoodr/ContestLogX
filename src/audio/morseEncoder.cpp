/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "audio/morseEncoder.h"
#include "audio/morseTable.h"

namespace clx::audio {

QString encodeChar(QChar c)
{
    const QChar up = c.toUpper();
    if (up == QLatin1Char(' ')) return QString();

    for (const MorseEntry& e : kMorseTable) {
        // Skip prosigns (their "text" starts with '<') — they're decode-only.
        if (!e.text.empty() && e.text[0] == '<') continue;
        if (e.text.size() != 1) continue;
        if (QChar(e.text[0]) == up) {
            return QString::fromUtf8(e.code.data(), static_cast<int>(e.code.size()));
        }
    }
    return QString();
}

} // namespace clx::audio
