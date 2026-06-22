/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * PracticeContentGenerator - produces text fragments for the CW Practice
 * audio source. Two modes:
 *   - Ragchew: random CQ calls and DX rag-chew exchanges from a template
 *     pool with substitution slots (call, name, QTH, rig, power, etc.)
 *   - Contest: exchanges built against the currently-loaded contest's
 *     `getDefaultSentExchange()` format, with RST fixed at 5NN.
 */

#ifndef AUDIO_PRACTICECONTENTGENERATOR_H
#define AUDIO_PRACTICECONTENTGENERATOR_H

#include <QString>
#include <QStringList>
#include <random>

class ContestEngine;

namespace clx::audio {

enum class PracticeMode {
    Ragchew,
    Contest,
};

class PracticeContentGenerator {
public:
    PracticeContentGenerator();

    // ContestEngine is only consulted in Contest mode. Ragchew works with a
    // null pointer. Owned by the caller; generator holds a non-owning ref.
    void setContestEngine(const ContestEngine* engine) { m_engine = engine; }

    // Returns next ready-to-key text fragment for the given mode. Always
    // returns a non-empty uppercase string terminated by a trailing space
    // so the audio source can emit a word gap before the next fragment.
    QString nextFragment(PracticeMode mode);

private:
    QString nextRagchew();
    QString nextContest();

    QString randomCall();        // Realistic-looking random ham call
    QString randomName();        // First name from a pool
    QString randomState();       // US 2-letter state code
    QString randomRig();
    QString randomAntenna();
    QString randomPower();       // "100", "500", "1K", "QRP", ...
    int     randomSerial();      // Simulated received serial
    QString randomNamedMult(const ContestEngine* engine);

    std::mt19937 m_rng;
    const ContestEngine* m_engine = nullptr;
    int m_lastRagchewTemplate = -1;
};

} // namespace clx::audio

#endif // AUDIO_PRACTICECONTENTGENERATOR_H
