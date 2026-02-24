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

#include "ssbMemoriesWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QWidget>
#include <QLabel>

SsbMemoriesWidget::SsbMemoriesWidget(QWidget *parent)
    : QDockWidget("SSB Memories", parent)
{
    setupUi();
}

void SsbMemoriesWidget::setupUi()
{
    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    setWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // Create grid layout for 2 rows x 4 columns
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setSpacing(5);

    // Create F1-F8 buttons
    for (int i = 0; i < 8; i++) {
        m_memoryButtons[i] = new QPushButton(this);
        m_memoryButtons[i]->setMinimumHeight(50);
        m_memoryButtons[i]->setMinimumWidth(80);

        // Set object name to identify which button was clicked
        m_memoryButtons[i]->setObjectName(QString("F%1").arg(i + 1));

        connect(m_memoryButtons[i], &QPushButton::clicked,
                this, &SsbMemoriesWidget::onMemoryButtonClicked);

        // Add to grid: first 4 buttons in row 0, next 4 in row 1
        int row = i / 4;
        int col = i % 4;
        gridLayout->addWidget(m_memoryButtons[i], row, col);
    }

    mainLayout->addLayout(gridLayout);
    mainLayout->addStretch();

    // Initialize with empty memories
    for (int i = 0; i < 8; i++) {
        m_memories.append(SsbMemory{"", ""});
    }

    updateButtonLabels();
}

void SsbMemoriesWidget::setMemories(const QList<SsbMemory>& memories)
{
    m_memories = memories;

    // Ensure we have exactly 8 memories
    while (m_memories.size() < 8) {
        m_memories.append(SsbMemory{"", ""});
    }

    updateButtonLabels();
}

void SsbMemoriesWidget::updateButtonLabels()
{
    for (int i = 0; i < 8; i++) {
        QString label = QString("F%1").arg(i + 1);

        if (!m_memories[i].abbreviation.isEmpty()) {
            label += "\n" + m_memories[i].abbreviation;
        }

        m_memoryButtons[i]->setText(label);

        // Disable button if no text is set
        m_memoryButtons[i]->setEnabled(!m_memories[i].text.isEmpty());
    }
}

void SsbMemoriesWidget::triggerMemory(int memoryNumber)
{
    // memoryNumber is 0-7 (for F1-F8)
    if (memoryNumber >= 0 && memoryNumber < 8 && memoryNumber < m_memories.size()) {
        QString text = m_memories[memoryNumber].text;
        if (!text.isEmpty()) {
            emit memoryTriggered(memoryNumber + 1, text);  // +1 to match F-key numbering
        }
    }
}

void SsbMemoriesWidget::onMemoryButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    // Extract the memory number from object name (e.g., "F1" -> 1)
    QString objectName = button->objectName();
    if (objectName.startsWith("F")) {
        int memoryNumber = objectName.mid(1).toInt();

        if (memoryNumber >= 1 && memoryNumber <= 8) {
            int index = memoryNumber - 1;
            QString text = m_memories[index].text;

            if (!text.isEmpty()) {
                emit memoryTriggered(memoryNumber, text);
            }
        }
    }
}
