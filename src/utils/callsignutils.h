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
