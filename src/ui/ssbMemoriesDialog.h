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

#ifndef SSBMEMORIESDIALOG_H
#define SSBMEMORIESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QList>
#include <QPushButton>
#include "ssbMemory.h"

class SsbMemoriesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SsbMemoriesDialog(QWidget *parent = nullptr);
    ~SsbMemoriesDialog();

    QList<SsbMemory> getMemories() const;
    void setMemories(const QList<SsbMemory>& memories);

    // Contest-specific memory support
    void setContestMemories(const QList<SsbMemory>& memories);
    void setContestMode(bool contest);
    bool isContestMode() const;

private slots:
    void onSave();
    void onCancel();
    void onModeToggled();

private:
    void setupUi();
    void loadMemoriesToUi(const QList<SsbMemory>& memories);
    QList<SsbMemory> getMemoriesFromUi() const;

    // F1-F8 inputs
    QLineEdit *m_abbrevEdits[8];
    QTextEdit *m_textEdits[8];

    // Station/Contest radio buttons
    QRadioButton *m_stationRadio;
    QRadioButton *m_contestRadio;

    // Internal storage for both sets
    QList<SsbMemory> m_stationMemories;
    QList<SsbMemory> m_contestMemories;
    bool m_currentIsContest = false;
};

#endif // SSBMEMORIESDIALOG_H
