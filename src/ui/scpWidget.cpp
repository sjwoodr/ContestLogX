/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "scpWidget.h"
#include "debugLogger.h"
#include "settings.h"
#include <QVBoxLayout>
#include <QWidget>
#include <QHeaderView>
#include <QStackedWidget>
#include <QPushButton>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QFile>
#include <QCoreApplication>
#include <QDir>

ScpWidget::ScpWidget(QWidget *parent)
    : QDockWidget("Super Check Partial", parent)
{
    setupUi();
    setObjectName("ScpWidget");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFloating(false);
    
    // Update title based on current state
    updateTitle();
}

void ScpWidget::setupUi()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stack = new QStackedWidget(this);

    // Page 0: the results table (shown when SCP is enabled)
    m_callsignTable = new QTableWidget(this);
    m_callsignTable->setColumnCount(1);
    m_callsignTable->setMinimumHeight(100);
    m_callsignTable->setMaximumHeight(200);

    // Style as borderless table
    m_callsignTable->setStyleSheet("QTableWidget { border: none; gridline-color: transparent; }"
                                   "QTableWidget::item { padding: 0px; }");
    m_callsignTable->horizontalHeader()->setVisible(false);
    m_callsignTable->verticalHeader()->setVisible(false);
    m_callsignTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_callsignTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_callsignTable->setShowGrid(false);
    m_callsignTable->setAlternatingRowColors(true);

    m_callsignTable->setFocusPolicy(Qt::NoFocus);
    connect(m_callsignTable, &QTableWidget::cellDoubleClicked,
            this, &ScpWidget::onCellDoubleClicked);

    m_stack->addWidget(m_callsignTable);

    // Page 1: clickable "configure" button (shown when SCP is disabled)
    m_configureButton = new QPushButton("SCP is disabled - click to configure", this);
    m_configureButton->setFocusPolicy(Qt::NoFocus);
    m_configureButton->setCursor(Qt::PointingHandCursor);
    m_configureButton->setStyleSheet("QPushButton { border: none; padding: 20px; font-size: 10pt; "
                                     "color: palette(link); text-decoration: underline; }"
                                     "QPushButton:hover { color: palette(highlight); }");
    connect(m_configureButton, &QPushButton::clicked,
            this, &ScpWidget::configureRequested);
    m_stack->addWidget(m_configureButton);

    layout->addWidget(m_stack);

    setWidget(widget);
}

int ScpWidget::getColumnCount() const
{
    // Allow up to 8 characters per column, calculate based on widget width
    int availableWidth = m_callsignTable->width();
    if (availableWidth < 1) return 1;
    
    // Rough estimate: 8 characters + padding
    int charWidth = 8;  // pixels per character (approximate for monospace)
    int columnWidth = charWidth * 8 + 10;  // 8 chars + padding
    
    int cols = qMax(1, availableWidth / columnWidth);
    return cols;
}

void ScpWidget::updateResults(const QStringList& callsigns)
{
    m_callsignTable->setRowCount(0);
    
    if (callsigns.isEmpty()) {
        return;
    }
    
    int columnCount = getColumnCount();
    m_callsignTable->setColumnCount(columnCount);
    
    int row = 0;
    int col = 0;
    
    for (const QString& call : callsigns) {
        if (col == 0) {
            m_callsignTable->insertRow(row);
        }
        
        QTableWidgetItem *item = new QTableWidgetItem(call);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_callsignTable->setItem(row, col, item);
        
        col++;
        if (col >= columnCount) {
            col = 0;
            row++;
        }
    }
    
    layoutTable();
}

void ScpWidget::layoutTable()
{
    // Set equal column widths
    int columnCount = m_callsignTable->columnCount();
    if (columnCount > 0) {
        int colWidth = m_callsignTable->width() / columnCount;
        for (int i = 0; i < columnCount; ++i) {
            m_callsignTable->setColumnWidth(i, colWidth);
        }
    }
    
    // Set row heights
    m_callsignTable->resizeRowsToContents();
}

void ScpWidget::clearResults()
{
    m_callsignTable->setRowCount(0);
}

QString ScpWidget::getSelectedCallsign() const
{
    QTableWidgetItem *item = m_callsignTable->currentItem();
    return item ? item->text() : QString();
}

void ScpWidget::setSearchPrefix(const QString& prefix)
{
    Q_UNUSED(prefix);
}

void ScpWidget::updateTitle()
{
    QString title = "Super Check Partial";
    bool enabled = Settings::instance().getScpEnabled();

    // Also require that master.scp exists
    if (enabled) {
        QString dataPath = Settings::getUserDataPath();
        QString scpPath = QDir(dataPath).filePath("master.scp");
        if (!QFile::exists(scpPath)) enabled = false;
    }

    if (enabled) {
        setWindowTitle(title);
        if (m_stack) m_stack->setCurrentWidget(m_callsignTable);
        if (m_configureButton) m_configureButton->setText("SCP is disabled - click to configure");
    } else {
        // Determine the specific reason so the button text is informative
        QString reason = "SCP is disabled - click to configure";
        if (!Settings::instance().getScpEnabled()) {
            reason = "SCP is disabled - click to enable";
        } else {
            reason = "master.scp not found - click to download";
        }
        setWindowTitle(title + " - disabled");
        if (m_stack) m_stack->setCurrentWidget(m_configureButton);
        if (m_configureButton) m_configureButton->setText(reason);
    }
}

void ScpWidget::resizeEvent(QResizeEvent *event)
{
    QDockWidget::resizeEvent(event);
    
    // Recalculate columns if width changed significantly
    int newColCount = getColumnCount();
    if (newColCount != m_callsignTable->columnCount() && m_callsignTable->rowCount() > 0) {
        QStringList calls;
        for (int row = 0; row < m_callsignTable->rowCount(); ++row) {
            for (int col = 0; col < m_callsignTable->columnCount(); ++col) {
                QTableWidgetItem *item = m_callsignTable->item(row, col);
                if (item && !item->text().isEmpty()) {
                    calls.append(item->text());
                }
            }
        }
        updateResults(calls);
    }
}

void ScpWidget::onCellDoubleClicked(int row, int column)
{
    QTableWidgetItem *item = m_callsignTable->item(row, column);
    if (item && !item->text().isEmpty()) {
        emit callsignSelected(item->text());
        DebugLogger::instance().log("ScpWidget", QString("Callsign selected: %1").arg(item->text()));
    }
}

