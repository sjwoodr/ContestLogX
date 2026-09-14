/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef MODEUTILS_H
#define MODEUTILS_H

#include <QString>

// Rigs report the sideband and filter along with the mode itself: a Yaesu
// FT-710 in CW says "CW-U" or "CW-L", RTTY says "RTTY-L" / "RTTY-U", data
// says "DATA-L" / "DATA-U". CLX compares mode strings against the contest's
// allowed-mode list and against its own scoring categories, so those variants
// have to be folded down to one canonical name before they reach a QSO
// record - otherwise a perfectly legal CW contact refuses to log, and any
// that slip through score as phone.
class ModeUtils
{
public:
    // Canonical CLX mode for any rig-, file-, or user-supplied mode string.
    // Unknown modes come back trimmed and upper-cased but otherwise untouched.
    static QString normalizeMode(const QString& mode);

    // True for any flavour of data mode (RTTY, USB-D/LSB-D, FT8, PSK, ...).
    // Contests name their digital mode differently than rigs do, so logging
    // asks this rather than matching the mode string directly.
    static bool isDigital(const QString& mode);
};

#endif // MODEUTILS_H
