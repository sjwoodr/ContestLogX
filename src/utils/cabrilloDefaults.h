/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CABRILLODEFAULTS_H
#define CABRILLODEFAULTS_H

#include <QJsonObject>
#include <QMap>
#include <QString>

// Works out Cabrillo 3.0 header values from what the operator already told
// CLX in contest setup (station class and userPrompt answers), so the
// Cabrillo export dialog opens with their category, power, mode, etc. already
// chosen instead of whatever was used for the last contest.
class CabrilloDefaults
{
public:
    // Header name ("CATEGORY-POWER", "IOTA-ISLAND-NAME", ...) -> value.
    // Only headers setup actually decides are present.
    //
    // 1. logging.cabrillo.headerMap in the contest definition, for headers a
    //    contest needs that setup can't be read for generically:
    //      "CATEGORY-LOCATION": {"promptId": "stationType",
    //                            "values": {"ISLAND": "NON-DXPEDITION", ...}}
    //      "IOTA-ISLAND-NAME":  {"promptId": "islandName"}   (answer as-is)
    //      "CATEGORY-STATION":  {"stationClass": true, "values": {"R": "ROVER"}}
    // 2. Otherwise inferred from the station class id and the answers to the
    //    category prompts (operatingCategory, operatingClass, powerCategory,
    //    contestMode, modeCategory - never the location prompts), read as
    //    tokens: SOHP-AB, SO_ASSISTED_RTTY, MOBILE-SO, SOSB-20, M2, ...
    //    (CATEGORY-OPERATOR, -ASSISTED, -POWER, -MODE, -BAND, -TRANSMITTER,
    //    -STATION).
    static QMap<QString, QString> fromSetup(const QJsonObject& contestDef,
                                            const QString& stationClass,
                                            const QMap<QString, QString>& promptValues);

    // Map a value saved by older CLX versions to its Cabrillo 3.0 equivalent
    // (CATEGORY-OPERATOR SINGLE -> SINGLE-OP, -ASSISTED YES -> ASSISTED,
    // -BAND ALL-BAND -> ALL, -MODE DIGITAL/FT8 -> DIGI). Other values pass
    // through unchanged.
    static QString toCabrillo3(const QString& header, const QString& value);
};

#endif // CABRILLODEFAULTS_H
