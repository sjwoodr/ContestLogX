/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef DEBUGLOGVIEWER_H
#define DEBUGLOGVIEWER_H

#include <QWidget>
#include <QFile>

class QPlainTextEdit;
class QCheckBox;
class QPushButton;
class QTimer;
class QLabel;

// Standalone (non-dockable) window that tails the DebugLogger's file.
// Inherits QWidget (not QDialog) so window managers treat it as a
// regular top-level window with its own taskbar entry - the operator
// can leave it open in the background and switch back to the main
// CLX window without dismissing it.
//
// Polls every kPollIntervalMs and appends any new bytes to a read-only
// QPlainTextEdit. Auto-scrolls to the bottom on new input unless the
// operator unchecks the auto-scroll box (useful when scrolling back to
// inspect older lines). The Clear button wipes the display only - it
// does NOT truncate the underlying log file, since DebugLogger holds
// it open for write.
class DebugLogViewer : public QWidget
{
    Q_OBJECT

public:
    explicit DebugLogViewer(QWidget* parent = nullptr);
    ~DebugLogViewer() override;

private slots:
    void poll();
    void onClearClicked();
    void onCopyClicked();

private:
    void openLogFile();

    QPlainTextEdit* m_view = nullptr;
    QCheckBox*      m_autoScrollCheck = nullptr;
    QPushButton*    m_clearButton = nullptr;
    QPushButton*    m_copyButton = nullptr;
    QPushButton*    m_closeButton = nullptr;
    QLabel*         m_pathLabel = nullptr;
    QTimer*         m_pollTimer = nullptr;

    QFile           m_logFile;
    qint64          m_lastPos = 0;
    QString         m_currentPath;
};

#endif // DEBUGLOGVIEWER_H
