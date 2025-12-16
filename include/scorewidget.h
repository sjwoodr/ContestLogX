/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025, by Steve Woodruff, N9OH
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
    void clear();
    void resetScore();

private:
    QTableWidget* m_scoreTable;
    QLabel* m_contestScoreLabel;
    QStringList m_contestBands;
    
    void setupTable();
    void rebuildTable();
    QString getBandDisplayOrder(int index) const;
};

#endif // SCOREWIDGET_H
