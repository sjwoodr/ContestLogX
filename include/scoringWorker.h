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

#ifndef SCORINGWORKER_H
#define SCORINGWORKER_H

#include <QObject>
#include <QList>
#include "qsoRecord.h"
#include "contestEngine.h"

class ScoringWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit ScoringWorker(QList<QsoRecord> qsos, ContestEngine* contestEngine, 
                          const QString& myCallsign, QObject *parent = nullptr);
    
public slots:
    void doScore();
    
signals:
    void progressUpdated(int current, int total);
    void scoringComplete(QList<QsoRecord> scoredQsos, bool success);
    
private:
    QList<QsoRecord> m_qsos;
    ContestEngine* m_contestEngine;
    QString m_myCallsign;
};

#endif // SCORINGWORKER_H
