/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SCOREWIDGET_H
#define SCOREWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include "contestEngine.h"

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
