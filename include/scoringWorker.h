/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
