/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef SSBMEMORIESDIALOG_H
#define SSBMEMORIESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QComboBox>
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
    void onCopyFromStation();

private:
    void setupUi();
    void loadMemoriesToUi(const QList<SsbMemory>& memories);
    QList<SsbMemory> getMemoriesFromUi() const;

    // F1-F8 inputs
    QLineEdit *m_abbrevEdits[8];
    QTextEdit *m_textEdits[8];
    QComboBox *m_roleCombo[8];

    // Station/Contest radio buttons
    QRadioButton *m_stationRadio;
    QRadioButton *m_contestRadio;
    QPushButton *m_copyFromStationButton;

    // Internal storage for both sets
    QList<SsbMemory> m_stationMemories;
    QList<SsbMemory> m_contestMemories;
    bool m_currentIsContest = false;
};

#endif // SSBMEMORIESDIALOG_H
