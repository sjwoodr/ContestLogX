/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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

class RigInterface;

class CWWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CWWindow(RigInterface* rigClient, QWidget *parent = nullptr);
    ~CWWindow();

    int getCurrentWPM() const { return wpmSpinBox->value(); }
    void setMemories(const QList<CwMemory>& memories);
    void setMemoriesFont(const QFont& font);
    void setRigClient(RigInterface* client) { rigClient = client; }

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
    RigInterface* rigClient;
    QList<CwMemory> memories;
};

#endif // CWWINDOW_H
