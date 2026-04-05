/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CWMEMORIESDIALOG_H
#define CWMEMORIESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QList>
#include <QPushButton>
#include "cwMemory.h"

class CwMemoriesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CwMemoriesDialog(QWidget *parent = nullptr);
    ~CwMemoriesDialog();

    QList<CwMemory> getMemories() const;
    void setMemories(const QList<CwMemory>& memories);

    // Contest-specific memory support
    void setContestMemories(const QList<CwMemory>& memories);
    void setContestMode(bool contest);
    bool isContestMode() const;

    // SN formatting options
    int getSnPadding() const;       // 1, 2, or 3
    void setSnPadding(int digits);
    bool getSnCutNumbers() const;
    void setSnCutNumbers(bool enabled);

private slots:
    void onSave();
    void onCancel();
    void onModeToggled();
    void onCopyFromStation();

private:
    void setupUi();
    void loadMemoriesToUi(const QList<CwMemory>& memories);
    QList<CwMemory> getMemoriesFromUi() const;

    // F1-F8 inputs
    QLineEdit *m_abbrevEdits[8];
    QTextEdit *m_textEdits[8];
    QComboBox *m_roleCombo[8];

    // Station/Contest radio buttons
    QRadioButton *m_stationRadio;
    QRadioButton *m_contestRadio;
    QPushButton *m_copyFromStationButton;

    // Internal storage for both sets
    QList<CwMemory> m_stationMemories;
    QList<CwMemory> m_contestMemories;
    bool m_currentIsContest = false;

    // SN formatting widgets
    QComboBox *m_snPaddingCombo;
    QCheckBox *m_snCutNumbersCheck;
};

#endif // CWMEMORIESDIALOG_H
