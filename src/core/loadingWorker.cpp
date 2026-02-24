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
