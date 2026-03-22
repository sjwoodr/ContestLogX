/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "scpLineEdit.h"
#include "scpWidget.h"
#include "superCheckPartial.h"
#include "debugLogger.h"
#include "settings.h"
#include <QKeyEvent>
#include <QTimer>

ScpLineEdit::ScpLineEdit(QWidget *parent)
    : QLineEdit(parent), m_scpWidget(nullptr), m_scpEnabled(false)
{
    // Use a timer to debounce SCP searches (avoid searching on every character)
    QTimer *searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(200);  // 200ms debounce
    connect(searchTimer, &QTimer::timeout, this, &ScpLineEdit::performScpSearch);
    
    connect(this, &QLineEdit::textChanged, searchTimer, [searchTimer]() {
        searchTimer->stop();
        searchTimer->start();
    });
    
    setObjectName("ScpLineEdit");
}

void ScpLineEdit::setScpWidget(ScpWidget *widget)
{
    m_scpWidget = widget;
    if (m_scpWidget) {
        // Connect widget's selection signal
        connect(m_scpWidget, &ScpWidget::callsignSelected, 
                this, &ScpLineEdit::onScpCallsignSelected);
    }
}

void ScpLineEdit::setScpEnabled(bool enabled)
{
    m_scpEnabled = enabled;
    if (!enabled && m_scpWidget) {
        m_scpWidget->clearResults();
    }
    DebugLogger::instance().log("ScpLineEdit", 
        QString("SCP search %1").arg(enabled ? "enabled" : "disabled"));
}

void ScpLineEdit::performScpSearch()
{
    // Always check the current setting from Settings
    bool scpEnabled = Settings::instance().getScpEnabled();
    if (!scpEnabled || !m_scpWidget) {
        return;
    }
    
    QString searchText = text().trimmed();
    
    if (searchText.isEmpty()) {
        m_scpWidget->clearResults();
        return;
    }
    
    // Search SCP database for matching callsigns (limit to 20 results)
    QStringList matches = SuperCheckPartial::instance().search(searchText, 20);
    
    if (matches.isEmpty()) {
        m_scpWidget->clearResults();
    } else {
        m_scpWidget->updateResults(matches);
        m_scpWidget->setSearchPrefix(searchText);
        DebugLogger::instance().log("ScpLineEdit", 
            QString("SCP search for '%1' found %2 matches").arg(searchText).arg(matches.size()));
    }
}

void ScpLineEdit::onScpCallsignSelected(const QString& callsign)
{
    // Auto-fill the call field from SCP widget selection
    setText(callsign);
    DebugLogger::instance().log("ScpLineEdit", QString("SCP selected: %1").arg(callsign));
}

void ScpLineEdit::keyPressEvent(QKeyEvent *event)
{
    // Allow standard key handling
    QLineEdit::keyPressEvent(event);
}

