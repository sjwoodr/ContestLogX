/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef IOTAUTILS_H
#define IOTAUTILS_H

#include <QString>

// IOTA (Islands On The Air) references: a continent code (AF AN AS EU NA OC
// SA) and a group number. Operators send and type them several ways - EU5,
// EU005, EU-005, eu-5 - but the RSGB IOTA Contest logs them as "EU-005", and
// they must compare equal for multipliers and same-island scoring.
class IotaUtils
{
public:
    // Canonical "XX-NNN" form, or an empty string if `ref` is not an IOTA
    // reference (including an empty exchange from a World station).
    static QString normalizeRef(const QString& ref);
};

#endif // IOTAUTILS_H
