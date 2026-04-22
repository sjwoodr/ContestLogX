/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * Text-to-Morse encoder used by the Practice audio source to synthesize
 * dot/dash patterns that get fed into both the decoder pipeline and the
 * playback path. Mirrors the existing decoder morseTable.h in reverse.
 */

#ifndef AUDIO_MORSEENCODER_H
#define AUDIO_MORSEENCODER_H

#include <QChar>
#include <QString>

namespace clx::audio {

// Returns dot/dash pattern (".", "-") for the given character, or empty if
// not encodable. Uppercases input. Space → empty (the caller handles word
// gap timing externally).
QString encodeChar(QChar c);

} // namespace clx::audio

#endif // AUDIO_MORSEENCODER_H
