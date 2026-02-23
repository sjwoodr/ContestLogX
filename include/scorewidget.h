/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef SCOREWIDGET_H
#define SCOREWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include "contestengine.h"

class ScoreWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScoreWidget(QWidget *parent = nullptr);
    
    void updateScore(const ContestEngine::ContestScore& score);
    void setContestBands(const QStringList& bands);
    void setMultCategories(const QStringList& categories) { m_multCategories = categories; }
    void clear();
    void resetScore();
    int getFinalScore() const { return m_finalScore; }
    void setBaseFont(const QFont& font);

private:
    QTableWidget* m_scoreTable;
    QLabel* m_titleLabel;
    QLabel* m_contestScoreLabel;
    QLabel* m_multsSummaryLabel;
    QStringList m_contestBands;
    QStringList m_multCategories;
    int m_finalScore = 0;
    
    void setupTable();
    void rebuildTable();
    QString getBandDisplayOrder(int index) const;
    void updateMultsSummary(const ContestEngine::ContestScore& score);
};

#endif // SCOREWIDGET_H
