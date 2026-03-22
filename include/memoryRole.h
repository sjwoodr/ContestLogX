/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef MEMORYROLE_H
#define MEMORYROLE_H

#include <QString>

/**
 * @brief Role a memory slot plays in the Run/S&P ENTER-key sequence.
 *
 * Roles are optional — a memory with NoRole behaves exactly as before.
 * At most one memory slot should carry each role; if multiple carry the
 * same role the first match wins.
 */
enum class MemoryRole {
    NoRole,      ///< Normal standalone memory — not part of Enter sequence
    CQ,          ///< Sent in Run mode when call field is empty (e.g. "CQ TEST {MYCALL}")
    MyCall,      ///< Sent in S&P mode on first Enter press (e.g. "{MYCALL}")
    RunExchange, ///< Run mode exchange — typically includes {CALL} (e.g. "{CALL} 5NN {SN}")
    SPExchange,  ///< S&P exchange — your part only, no {CALL} (e.g. "5NN {SN}")
    TU           ///< Sent after logging in Run mode (e.g. "TU {MYCALL}")
};

inline QString memoryRoleToString(MemoryRole role)
{
    switch (role) {
        case MemoryRole::CQ:          return "CQ";
        case MemoryRole::MyCall:      return "MyCall";
        case MemoryRole::RunExchange: return "RunExchange";
        case MemoryRole::SPExchange:  return "SPExchange";
        case MemoryRole::TU:          return "TU";
        default:                      return "";
    }
}

inline MemoryRole memoryRoleFromString(const QString& s)
{
    if (s == "CQ")          return MemoryRole::CQ;
    if (s == "MyCall")      return MemoryRole::MyCall;
    if (s == "RunExchange") return MemoryRole::RunExchange;
    if (s == "SPExchange")  return MemoryRole::SPExchange;
    if (s == "TU")          return MemoryRole::TU;
    // Legacy: plain "Exchange" maps to RunExchange for backward compatibility
    if (s == "Exchange")    return MemoryRole::RunExchange;
    return MemoryRole::NoRole;
}

#endif // MEMORYROLE_H
