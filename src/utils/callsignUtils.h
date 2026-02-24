/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CALLSIGNUTILS_H
#define CALLSIGNUTILS_H

#include <QString>

class CallsignUtils
{
public:
    // Extract the country prefix from a callsign
    static QString getCountryPrefix(const QString& callsign);
    
    // Get the country name (simplified - major countries only)
    static QString getCountry(const QString& callsign);
    
    // Get the continent code (NA, EU, AS, OC, SA, AF)
    static QString getContinent(const QString& callsign);
    
    // Check if two callsigns are from the same country
    static bool isSameCountry(const QString& call1, const QString& call2);
    
    // Check if two callsigns are from the same continent
    static bool isSameContinent(const QString& call1, const QString& call2);
};

#endif // CALLSIGNUTILS_H
