/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * Shared types for the CW decoder audio subsystem (SPEC-005).
 */

#ifndef AUDIO_AUDIOTYPES_H
#define AUDIO_AUDIOTYPES_H

#include <QtGlobal>
#include <QChar>
#include <cstdint>

namespace clx::audio {

// Which radio owns a decoder session. In non-SO2R mode only Left is used.
enum class RadioSide { Left, Right };

// Fixed DSP constants used across the audio subsystem. Changing these
// requires coordinated updates in binChannel/audioCapture/ring buffer sizing.
constexpr int kSampleRateHz    = 8000;
constexpr int kBlockSamples    = 80;       // 10 ms at 8 kHz
constexpr int kRingBufferSamples = 8000;   // 1 second of audio at 8 kHz
constexpr int kDotLengthWindow = 16;       // rolling median window for WPM
constexpr int kDefaultBinCount = 6;
constexpr int kDefaultPassbandLowHz  = 400;
constexpr int kDefaultPassbandHighHz = 1000;
constexpr int kDefaultWpmMin = 5;
constexpr int kDefaultWpmMax = 60;
constexpr float kDefaultSquelch = 0.05f;
constexpr int kDefaultPttGraceMs = 250;
constexpr int kMaxBinCount = 16;
constexpr int kMinBinSpacingHz = 50;       // lower bound to keep bins resolvable at 10 ms blocks
constexpr int kTextBufferCapChars = 10000; // per-bin scrollback cap

// Per-bin lock state reported by the decoder.
enum class LockState { NoLock, Locked };

// Mute-state bookkeeping carried inside a DecoderSession / worker.
struct MuteState {
    bool rigPttActive = false;               // set by pttStateChanged(bool)
    qint64 internalSendMuteUntilMs = 0;      // monotonic ms; 0 = not in internal mute
    bool pttSignalReceivedEver = false;      // tracks FR-019b fallback log
    bool isMuted(qint64 nowMs) const {
        return rigPttActive || (internalSendMuteUntilMs != 0 && nowMs < internalSendMuteUntilMs);
    }
};

// The unit on the audio ring buffer. Fixed-size to avoid allocations in the
// capture callback hot path.
struct AudioBlock {
    int16_t samples[kBlockSamples] = {};
    qint64  captureTimestampMs = 0;
};

// Classification of a clickable token span within a BinChannel's text buffer.
enum class TokenKind { Callsign, Rst };

} // namespace clx::audio

#endif // AUDIO_AUDIOTYPES_H
