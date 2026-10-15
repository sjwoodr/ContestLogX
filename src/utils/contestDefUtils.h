/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CONTESTDEFUTILS_H
#define CONTESTDEFUTILS_H

#include <QJsonObject>
#include <QString>
#include <functional>

// Questions answered from a contest definition (contests/*.json) alone, kept
// free of the scoring engine so they can be unit-tested against the shipped
// definitions.
class ContestDefUtils
{
public:
    // Looks up the operator's answer to a contest-setup userPrompt by id.
    using PromptLookup = std::function<QString(const QString& promptId)>;

    // True when the operator sends a serial number in this contest, given
    // their station class and setup answers. A serial is sent when:
    //  - the selected station class's exchangeSent.type is "serial" (ARRL 10m
    //    DX class, YBDX), or
    //  - a sent exchange field has type "serial" and its log column is not
    //    hidden by a qsoFields visibleWhen for the current setup (OK-OM sends
    //    a serial only from DX stations; OK/OM stations send a county).
    static bool sendsSerialNumber(const QJsonObject& contestDef,
                                  const QString& stationClass,
                                  const PromptLookup& promptValue);
};

#endif // CONTESTDEFUTILS_H
