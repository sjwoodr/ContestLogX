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

// DSP constants used across the audio subsystem.
//
// The sample rate is now DYNAMIC — it comes from the device's preferred
// format (e.g., 44100 or 48000 Hz) rather than a fixed 8 kHz with
// nearest-neighbor decimation. The decimation path was catastrophically
// aliasing above-4-kHz content (speaker hiss, harmonic distortion,
// ambient noise) back into the CW detection band, raising the effective
// noise floor and wedging the Schmitt trigger. Working at native rate
// eliminates that entire class of problem.
//
// kSampleRateHz is kept as the FALLBACK rate used only when a device
// reports an unusable format. In practice, Qt6::Multimedia will always
// give us a sane sample rate — typically 44100 or 48000 Hz.
constexpr int kSampleRateHz    = 48000;    // default / preferred (device may override)
constexpr int kBlockDurationMs = 10;       // DSP block size in ms
constexpr int kRingBufferSeconds = 1;      // audio ring buffer size in seconds
constexpr int kDotLengthWindow = 16;       // rolling median window for WPM

// Compute block samples for a given sample rate.
inline int blockSamplesForRate(int sampleRateHz) {
    return (sampleRateHz * kBlockDurationMs) / 1000;
}
constexpr int kDefaultBinCount = 6;
constexpr int kDefaultPassbandLowHz  = 400;
constexpr int kDefaultPassbandHighHz = 1000;
constexpr int kDefaultWpmMin = 5;
constexpr int kDefaultWpmMax = 60;
// Default squelch sits just above a typical noise floor with the 5×-sensitive
// normalization in BinChannel; Schmitt hysteresis halves this for the off-threshold.
constexpr float kDefaultSquelch = 0.10f;
constexpr int kDefaultPttGraceMs = 250;
constexpr int kMaxBinCount = 16;
// Minimum bin spacing. At 8 kHz / 80-sample Goertzel, bin resolution is
// 100 Hz, so spacings below that produce overlapping bins — but overlap is
// not pathological (just redundant detection), so this floor is set just
// low enough to reject configurations that produce effectively duplicate
// bins without blocking legitimate narrow-passband use (e.g., 6 bins over
// a 250 Hz range gives 42 Hz spacing, which is useful for tuning in on a
// specific signal).
constexpr int kMinBinSpacingHz = 20;
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

// (The old fixed-size AudioBlock struct was removed; the ring buffer
// now stores raw int16 samples directly at whatever rate the device
// provides, and consumers read back dynamically-sized blocks based on
// the actual sample rate.)

// Classification of a clickable token span within a BinChannel's text buffer.
enum class TokenKind { Callsign, Rst };

} // namespace clx::audio

#endif // AUDIO_AUDIOTYPES_H
