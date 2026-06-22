/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "debugLogViewer.h"
#include "debugLogger.h"

#include <QPlainTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFontDatabase>
#include <QScrollBar>
#include <QFileInfo>
#include <QClipboard>
#include <QGuiApplication>

namespace {
constexpr int kPollIntervalMs = 1500;
// Cap the in-view buffer so a long session doesn't grow without bound;
// the file on disk is unaffected by this.
constexpr int kMaxBlocks = 20000;
}

DebugLogViewer::DebugLogViewer(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(tr("Debug Log Viewer"));
    // Independent top-level window - operator can leave this running
    // in the background while continuing to use CLX. The QWidget +
    // Qt::Window combination (rather than QDialog) gives us a regular
    // window with its own taskbar entry, not a dialog that the WM
    // forces above the parent.
    resize(900, 500);

    auto* outer = new QVBoxLayout(this);

    m_pathLabel = new QLabel(this);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outer->addWidget(m_pathLabel);

    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setUndoRedoEnabled(false);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setMaximumBlockCount(kMaxBlocks);
    m_view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    outer->addWidget(m_view, /*stretch*/ 1);

    auto* row = new QHBoxLayout;
    m_clearButton = new QPushButton(tr("Clear"), this);
    m_copyButton = new QPushButton(tr("Copy"), this);
    m_copyButton->setToolTip(tr("Copy the visible log buffer to the clipboard"));
    m_autoScrollCheck = new QCheckBox(tr("Auto-scroll"), this);
    m_autoScrollCheck->setChecked(true);
    m_closeButton = new QPushButton(tr("Close"), this);
    row->addWidget(m_clearButton);
    row->addWidget(m_copyButton);
    row->addWidget(m_autoScrollCheck);
    row->addStretch();
    row->addWidget(m_closeButton);
    outer->addLayout(row);

    connect(m_clearButton, &QPushButton::clicked, this, &DebugLogViewer::onClearClicked);
    connect(m_copyButton, &QPushButton::clicked, this, &DebugLogViewer::onCopyClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::close);

    openLogFile();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &DebugLogViewer::poll);
    m_pollTimer->start();

    // Prime the view with whatever's already in the file.
    poll();
}

DebugLogViewer::~DebugLogViewer()
{
    if (m_logFile.isOpen()) m_logFile.close();
}

void DebugLogViewer::openLogFile()
{
    m_currentPath = DebugLogger::instance().logFilePath();
    if (m_currentPath.isEmpty()) {
        m_pathLabel->setText(tr("Log file: (not initialized)"));
        return;
    }
    const QString abs = QFileInfo(m_currentPath).absoluteFilePath();
    m_pathLabel->setText(tr("Log file: %1").arg(abs));

    if (m_logFile.isOpen()) m_logFile.close();
    m_logFile.setFileName(m_currentPath);
    if (!m_logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_view->appendPlainText(
            tr("[viewer] Could not open log file: %1").arg(abs));
        return;
    }
    m_lastPos = 0;
}

void DebugLogViewer::poll()
{
    if (!m_logFile.isOpen()) {
        // Logger may have initialized after the viewer opened; retry.
        openLogFile();
        if (!m_logFile.isOpen()) return;
    }

    const qint64 size = m_logFile.size();
    if (size < m_lastPos) {
        // File got truncated (e.g., a re-init). Restart from the top.
        m_lastPos = 0;
        m_view->clear();
    }
    if (size == m_lastPos) return;

    if (!m_logFile.seek(m_lastPos)) return;
    const QByteArray chunk = m_logFile.readAll();
    m_lastPos = m_logFile.pos();
    if (chunk.isEmpty()) return;

    // Strip the single trailing newline so appendPlainText doesn't
    // produce a blank block after each batch.
    QString text = QString::fromUtf8(chunk);
    if (text.endsWith(QLatin1Char('\n'))) text.chop(1);
    if (text.isEmpty()) return;

    const bool atBottom = m_autoScrollCheck->isChecked();
    m_view->appendPlainText(text);
    if (atBottom) {
        QScrollBar* bar = m_view->verticalScrollBar();
        if (bar) bar->setValue(bar->maximum());
    }
}

void DebugLogViewer::onClearClicked()
{
    // Clear the displayed text only - the underlying log file is held
    // open for write by DebugLogger and shouldn't be truncated from
    // here. Subsequent polls continue from m_lastPos so we don't
    // re-append history the operator just cleared.
    m_view->clear();
}

void DebugLogViewer::onCopyClicked()
{
    // Copy the entire in-view buffer (everything currently in the
    // QPlainTextEdit, capped by maxBlockCount), not just the lines
    // visible in the viewport - the latter would be near-useless for
    // sharing context. Operator can Clear → repro → Copy to capture
    // a focused snippet.
    if (QClipboard* cb = QGuiApplication::clipboard()) {
        cb->setText(m_view->toPlainText());
    }
}
