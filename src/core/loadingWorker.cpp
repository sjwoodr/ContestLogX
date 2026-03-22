/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "loadingWorker.h"
#include "fileHandler.h"

LoadingWorker::LoadingWorker(const QString& fileName, QObject *parent)
    : QObject(parent), m_fileName(fileName)
{
}

void LoadingWorker::doLoad()
{
    QList<QsoRecord> qsos;
    FileHandler fileHandler;
    
    if (fileHandler.load(m_fileName, qsos)) {
        emit loadingComplete(qsos, true, QString());
    } else {
        emit loadingComplete(qsos, false, fileHandler.lastError());
    }
}
