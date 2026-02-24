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

#include "scoringWorker.h"
#include "debugLogger.h"

ScoringWorker::ScoringWorker(QList<QsoRecord> qsos, ContestEngine* contestEngine,
                             const QString& myCallsign, QObject *parent)
    : QObject(parent)
    , m_qsos(qsos)
    , m_contestEngine(contestEngine)
    , m_myCallsign(myCallsign)
{
}

void ScoringWorker::doScore()
{
    if (!m_contestEngine) {
        DebugLogger::instance().log("ScoringWorker", "Error: ContestEngine is null");
        emit scoringComplete(m_qsos, false);
        return;
    }

    DebugLogger::instance().log("ScoringWorker",
        QString("Starting to score %1 QSOs on background thread").arg(m_qsos.count()));

    // Single O(n) pass — rescoreAll handles dupe detection, points, mults, and running score.
    m_contestEngine->rescoreAll(m_qsos, m_myCallsign);

    DebugLogger::instance().log("ScoringWorker",
        QString("Finished scoring %1 QSOs on background thread").arg(m_qsos.count()));

    emit scoringComplete(m_qsos, true);
}
