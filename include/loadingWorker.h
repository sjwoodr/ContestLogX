/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef LOADINGWORKER_H
#define LOADINGWORKER_H

#include <QObject>
#include <QString>
#include <QList>
#include "qsoRecord.h"

class LoadingWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit LoadingWorker(const QString& fileName, QObject *parent = nullptr);
    
public slots:
    void doLoad();
    
signals:
    void progressUpdated(int current, int total);
    void loadingComplete(QList<QsoRecord> qsos, bool success, QString errorMessage);
    
private:
    QString m_fileName;
};

#endif // LOADINGWORKER_H
