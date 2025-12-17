/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
 */

#include "scorewidget.h"
#include "debuglogger.h"
#include <QHeaderView>
#include <QFont>

ScoreWidget::ScoreWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    
    // Title with score
    QHBoxLayout* titleLayout = new QHBoxLayout();
    
    QLabel* titleLabel = new QLabel("Contest Score:", this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    titleLayout->addWidget(titleLabel);
    
    m_contestScoreLabel = new QLabel("0", this);
    QFont scoreFont = m_contestScoreLabel->font();
    scoreFont.setBold(true);
    scoreFont.setPointSize(scoreFont.pointSize() + 3);
    m_contestScoreLabel->setFont(scoreFont);
    titleLayout->addWidget(m_contestScoreLabel);
    titleLayout->addStretch();
    
    mainLayout->addLayout(titleLayout);
    
    // Multiplier summary (smaller font)
    m_multsSummaryLabel = new QLabel("", this);
    QFont summaryFont = m_multsSummaryLabel->font();
    summaryFont.setPointSize(summaryFont.pointSize() - 1);
    m_multsSummaryLabel->setFont(summaryFont);
    m_multsSummaryLabel->setStyleSheet("color: palette(text);");
    mainLayout->addWidget(m_multsSummaryLabel);
    
    // Score table
    m_scoreTable = new QTableWidget(this);
    setupTable();
    mainLayout->addWidget(m_scoreTable);
    
    setLayout(mainLayout);
}

void ScoreWidget::setupTable()
{
    // Default to all bands - will be updated when contest is loaded
    m_contestBands = {"160m", "80m", "40m", "20m", "15m", "10m", "6m", "2m", "70cm"};
    rebuildTable();
}

void ScoreWidget::rebuildTable()
{
    m_scoreTable->clear();
    m_scoreTable->setRowCount(m_contestBands.size());
    m_scoreTable->setColumnCount(4);
    
    QStringList headers;
    headers << "Band" << "CW" << "SSB" << "Digi";
    m_scoreTable->setHorizontalHeaderLabels(headers);
    
    // Set band names in first column
    for (int i = 0; i < m_contestBands.size(); ++i) {
        QTableWidgetItem* item = new QTableWidgetItem(m_contestBands[i]);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_scoreTable->setItem(i, 0, item);
    }
    
    // Initialize all cells to 0
    for (int row = 0; row < m_contestBands.size(); ++row) {
        for (int col = 1; col < 4; ++col) {
            QTableWidgetItem* item = new QTableWidgetItem("0");
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_scoreTable->setItem(row, col, item);
        }
    }
    
    // Configure table appearance
    m_scoreTable->horizontalHeader()->setStretchLastSection(true);
    m_scoreTable->verticalHeader()->setVisible(false);
    m_scoreTable->setAlternatingRowColors(true);
    m_scoreTable->setSelectionMode(QAbstractItemView::NoSelection);
    
    // Adjust column widths
    m_scoreTable->setColumnWidth(0, 60);  // Band
    m_scoreTable->setColumnWidth(1, 50);  // CW
    m_scoreTable->setColumnWidth(2, 50);  // SSB
    m_scoreTable->setColumnWidth(3, 50);  // Digi
}

void ScoreWidget::setContestBands(const QStringList& bands)
{
    m_contestBands = bands;
    rebuildTable();
}

void ScoreWidget::updateScore(const ContestEngine::ContestScore& score)
{
    DebugLogger::instance().log("ScoreWidget", 
        QString("updateScore called - bandStats size: %1").arg(score.bandStats.size()));
    for (auto it = score.bandStats.begin(); it != score.bandStats.end(); ++it) {
        DebugLogger::instance().log("ScoreWidget", 
            QString("  Received %1: CW=%2 SSB=%3 Digi=%4")
                .arg(it.key())
                .arg(it.value().cwQsos)
                .arg(it.value().ssbQsos)
                .arg(it.value().digitalQsos));
    }
    
    // Build map of band name to row index
    QMap<QString, int> bandRowMap;
    for (int i = 0; i < m_contestBands.size(); ++i) {
        bandRowMap[m_contestBands[i]] = i;
    }
    
    // Clear all cells first
    for (int row = 0; row < m_contestBands.size(); ++row) {
        for (int col = 1; col < 4; ++col) {
            m_scoreTable->item(row, col)->setText("0");
        }
    }
    
    // Update with actual data
    for (auto it = score.bandStats.begin(); it != score.bandStats.end(); ++it) {
        QString band = it.key();
        const ContestEngine::BandModeStats& stats = it.value();
        
        DebugLogger::instance().log("ScoreWidget", 
            QString("Looking for band '%1' in bandRowMap").arg(band));
        
        if (bandRowMap.contains(band)) {
            int row = bandRowMap[band];
            DebugLogger::instance().log("ScoreWidget", 
                QString("Found band '%1' at row %2, updating with CW=%3 SSB=%4 Digi=%5")
                    .arg(band).arg(row)
                    .arg(stats.cwQsos).arg(stats.ssbQsos).arg(stats.digitalQsos));
            m_scoreTable->item(row, 1)->setText(QString::number(stats.cwQsos));
            m_scoreTable->item(row, 2)->setText(QString::number(stats.ssbQsos));
            m_scoreTable->item(row, 3)->setText(QString::number(stats.digitalQsos));
        } else {
            DebugLogger::instance().log("ScoreWidget", 
                QString("Band '%1' NOT found in bandRowMap").arg(band));
        }
    }
    
    // Update contest score
    m_finalScore = score.contestScore;
    m_contestScoreLabel->setText(QString::number(score.contestScore));
    
    // Update multiplier summary
    updateMultsSummary(score);
}

void ScoreWidget::clear()
{
    // Reset all cells to 0
    for (int row = 0; row < m_contestBands.size(); ++row) {
        for (int col = 1; col < 4; ++col) {
            m_scoreTable->item(row, col)->setText("0");
        }
    }
    
    m_finalScore = 0;
    m_contestScoreLabel->setText("0");
}

void ScoreWidget::resetScore()
{
    clear();
}

QString ScoreWidget::getBandDisplayOrder(int index) const
{
    QStringList bands = {"70cm", "2m", "6m", "10m", "15m", "20m", "40m", "80m", "160m"};
    if (index >= 0 && index < bands.size()) {
        return bands[index];
    }
    return QString();
}

void ScoreWidget::updateMultsSummary(const ContestEngine::ContestScore& score)
{
    QStringList parts;
    
    if (m_multCategories.contains("namedMults") && score.namedMultCount > 0) {
        parts.append(QString("Named: %1").arg(score.namedMultCount));
    }
    if (m_multCategories.contains("dxcc") && score.dxccMultCount > 0) {
        parts.append(QString("DXCC: %1").arg(score.dxccMultCount));
    }
    if (m_multCategories.contains("ituRegions") && score.ituRegionMultCount > 0) {
        parts.append(QString("ITU: %1").arg(score.ituRegionMultCount));
    }
    
    QString summary = parts.join(" | ");
    m_multsSummaryLabel->setText(summary);
}
