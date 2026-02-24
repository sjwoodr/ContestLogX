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

#ifndef SSBMEMORIESWIDGET_H
#define SSBMEMORIESWIDGET_H

#include <QDockWidget>
#include <QPushButton>
#include <QList>
#include "ssbMemory.h"

class SsbMemoriesWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit SsbMemoriesWidget(QWidget *parent = nullptr);

    // Set the memories to display on buttons
    void setMemories(const QList<SsbMemory>& memories);

    // Trigger a specific memory (0-7 for F1-F8)
    void triggerMemory(int memoryNumber);

signals:
    // Emitted when a memory button is clicked
    void memoryTriggered(int memoryNumber, const QString& text);

private slots:
    void onMemoryButtonClicked();

private:
    void setupUi();
    void updateButtonLabels();

    QPushButton *m_memoryButtons[8];  // F1-F8 buttons
    QList<SsbMemory> m_memories;
};

#endif // SSBMEMORIESWIDGET_H
