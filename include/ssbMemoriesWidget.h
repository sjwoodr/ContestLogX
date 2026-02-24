/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
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
