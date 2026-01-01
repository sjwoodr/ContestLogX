/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#include "scpwidget.h"
#include "debuglogger.h"
#include <QVBoxLayout>
#include <QWidget>

ScpWidget::ScpWidget(QWidget *parent)
    : QDockWidget("Super Check Partial", parent)
{
    setupUi();
    setObjectName("ScpWidget");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFloating(false);
}

void ScpWidget::setupUi()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Callsign list only - no buttons, no labels
    m_callsignList = new QListWidget(this);
    m_callsignList->setMinimumHeight(100);
    m_callsignList->setMaximumHeight(200);
    connect(m_callsignList, &QListWidget::itemDoubleClicked, 
            this, &ScpWidget::onCallsignDoubleClicked);
    layout->addWidget(m_callsignList);
    
    setWidget(widget);
}

void ScpWidget::updateResults(const QStringList& callsigns)
{
    m_callsignList->clear();
    for (const QString& call : callsigns) {
        m_callsignList->addItem(call);
    }
}

void ScpWidget::clearResults()
{
    m_callsignList->clear();
}

QString ScpWidget::getSelectedCallsign() const
{
    QListWidgetItem *item = m_callsignList->currentItem();
    return item ? item->text() : QString();
}

void ScpWidget::setSearchPrefix(const QString& prefix)
{
    // No display needed - prefix is shown in the callsign entry field itself
    Q_UNUSED(prefix);
}

void ScpWidget::onCallsignDoubleClicked(QListWidgetItem* item)
{
    if (item) {
        emit callsignSelected(item->text());
        DebugLogger::instance().log("ScpWidget", QString("Callsign selected: %1").arg(item->text()));
    }
}

