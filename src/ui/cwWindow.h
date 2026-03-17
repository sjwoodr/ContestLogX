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

#ifndef CWWINDOW_H
#define CWWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QLabel>
#include <QFont>
#include "cwMemory.h"

class FlrigClient;

class CWWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CWWindow(FlrigClient* rigClient, QWidget *parent = nullptr);
    ~CWWindow();

    int getCurrentWPM() const { return wpmSpinBox->value(); }
    void setMemories(const QList<CwMemory>& memories);
    void setMemoriesFont(const QFont& font);

public:
    void sendCWText(const QString& text);

public slots:
    // Trigger a specific memory (0-7 for F1-F8)
    void onMemoryButton(int fKey);
    void onHalt();

signals:
    void wpmChanged(int wpm);
    void memoryTriggered(int fKey, const QString& text);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSendCW();
    void onClear();
    void onWpmChanged(int wpm);

private:
    
    QTextEdit* historyText;
    QLineEdit* inputLine;
    QPushButton* clearButton;
    QPushButton* haltButton;
    QSpinBox* wpmSpinBox;
    QPushButton* memoryButtons[8];  // F1-F8
    FlrigClient* rigClient;
    QList<CwMemory> memories;
};

#endif // CWWINDOW_H
