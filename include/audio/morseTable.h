/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * International Morse code lookup table for the CW decoder (SPEC-005).
 * Dots = '.', dashes = '-'. Prosigns map to multi-character expansions.
 */

#ifndef AUDIO_MORSETABLE_H
#define AUDIO_MORSETABLE_H

#include <QString>
#include <QChar>
#include <array>
#include <string_view>

namespace clx::audio {

struct MorseEntry {
    std::string_view code;    // dots/dashes
    std::string_view text;    // decoded text (single char or prosign expansion)
};

// International Morse + common prosigns used in contest CW.
// Length stays small so linear search is fine on the decode hot path.
inline constexpr std::array<MorseEntry, 56> kMorseTable = {{
    // Letters
    {".-",     "A"},   {"-...",   "B"},   {"-.-.",   "C"},   {"-..",    "D"},
    {".",      "E"},   {"..-.",   "F"},   {"--.",    "G"},   {"....",   "H"},
    {"..",     "I"},   {".---",   "J"},   {"-.-",    "K"},   {".-..",   "L"},
    {"--",     "M"},   {"-.",     "N"},   {"---",    "O"},   {".--.",   "P"},
    {"--.-",   "Q"},   {".-.",    "R"},   {"...",    "S"},   {"-",      "T"},
    {"..-",    "U"},   {"...-",   "V"},   {".--",    "W"},   {"-..-",   "X"},
    {"-.--",   "Y"},   {"--..",   "Z"},
    // Digits
    {"-----",  "0"},   {".----",  "1"},   {"..---",  "2"},   {"...--",  "3"},
    {"....-",  "4"},   {".....",  "5"},   {"-....",  "6"},   {"--...",  "7"},
    {"---..",  "8"},   {"----.",  "9"},
    // Punctuation common in contest exchanges
    {".-.-.-", "."},   {"--..--", ","},   {"..--..", "?"},   {"-....-", "-"},
    {"-..-.",  "/"},   {"-.--.",  "("},   {"-.--.-", ")"},   {".----.", "'"},
    {"-...-",  "="},   {".-.-.",  "+"},
    // Prosigns (contest-relevant)
    {".-.-",   "<AA>"},    // \n newline in some setups
    {".-...",  "<AS>"},    // wait
    {"-...-.-","<BK>"},    // break
    {"-.-..",  "<KN>"},    // go only
    {"...-.-", "<SK>"},    // end of contact
    {"-...-",  "<BT>"},    // same as = (pause)
    {".-.-.",  "<AR>"},    // same as + (end of message)
    {"........","<HH>"},   // error
    // CW short-form RST (operator shortcut - decodes as N for 9)
    {"-.",     "N"},       // already covered as letter N; N used for 9 in RST
    // (5NN, 4NN are composed at the token-parser layer, not here)
}};

// Linear lookup: pattern → decoded text. Returns empty QString if no match.
inline QString morseLookup(std::string_view code)
{
    for (const auto& e : kMorseTable) {
        if (e.code == code) {
            return QString::fromUtf8(e.text.data(), static_cast<int>(e.text.size()));
        }
    }
    return QString();
}

} // namespace clx::audio

#endif // AUDIO_MORSETABLE_H
