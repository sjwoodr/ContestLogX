/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "modeUtils.h"
#include <QHash>
#include <QSet>

QString ModeUtils::normalizeMode(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m.isEmpty())
        return m;

    // Mode spellings seen from flrig, rigctld and the rigs themselves.
    // USB, LSB and the -D data variants are deliberately absent: CLX treats
    // those as distinct and everything downstream already understands them.
    static const QHash<QString, QString> aliases = {
        // CW - the sideband (-U/-L), reverse (-R) and narrow (-N) flavours
        // are rig tuning details, not different modes.
        { "CWU",    "CW" }, { "CW-U",   "CW" },
        { "CWL",    "CW" }, { "CW-L",   "CW" },
        { "CWR",    "CW" }, { "CW-R",   "CW" },
        { "CWN",    "CW" }, { "CW-N",   "CW" },

        // RTTY - FSK on either sideband, normal or reverse.
        { "RTTYU",  "RTTY" }, { "RTTY-U", "RTTY" },
        { "RTTYL",  "RTTY" }, { "RTTY-L", "RTTY" },
        { "RTTYR",  "RTTY" }, { "RTTY-R", "RTTY" },
        { "FSK",    "RTTY" }, { "FSK-R",  "RTTY" },

        // Data over SSB - CLX calls these USB-D / LSB-D, and logging maps
        // them onto whichever digital mode the contest asks for.
        { "DATA-U", "USB-D" }, { "DATAU",  "USB-D" }, { "DATA-USB", "USB-D" },
        { "PKT-U",  "USB-D" }, { "PKTUSB", "USB-D" },
        { "DIGU",   "USB-D" }, { "DIG-U",  "USB-D" },
        { "DATA-L", "LSB-D" }, { "DATAL",  "LSB-D" }, { "DATA-LSB", "LSB-D" },
        { "PKT-L",  "LSB-D" }, { "PKTLSB", "LSB-D" },
        { "DIGL",   "LSB-D" }, { "DIG-L",  "LSB-D" },

        // Generic data spellings. DIGITAL is the name the contest files and
        // the scoring categories use, so everything else folds into it.
        { "DIG",  "DIGITAL" }, { "DIGI", "DIGITAL" },
        { "DATA", "DIGITAL" }, { "PKT",  "DIGITAL" },

        // Voice, with narrow/wide filter variants.
        { "SSB-U", "USB" }, { "USB-N", "USB" },
        { "SSB-L", "LSB" }, { "LSB-N", "LSB" },
        { "FM-N",  "FM"  }, { "FMN",   "FM"  }, { "FM-W", "FM" }, { "FMW", "FM" },
        { "AM-N",  "AM"  }, { "AMN",   "AM"  }, { "AM-S", "AM" },
    };

    return aliases.value(m, m);
}

bool ModeUtils::isDigital(const QString& mode)
{
    static const QSet<QString> digitalModes = {
        "DIGITAL", "RTTY", "USB-D", "LSB-D",
        "PSK", "PSK31", "PSK63", "PSK125",
        "FT8", "FT4", "JT65", "JT9", "JS8", "MSK144", "Q65", "FST4", "FST4W",
        "WSPR", "OLIVIA", "CONTESTIA", "MFSK", "HELL"
    };

    return digitalModes.contains(normalizeMode(mode));
}
