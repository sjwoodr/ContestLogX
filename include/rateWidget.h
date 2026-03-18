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

#ifndef RATEWIDGET_H
#define RATEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>

class QsoListModel;

class RateWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RateWidget(QWidget *parent = nullptr);

    void setModel(const QsoListModel* model);
    void setBaseFont(const QFont& font);

private slots:
    void onRefresh();

private:
    const QsoListModel* m_model = nullptr;
    QTimer* m_timer;

    QLabel* m_rate60Label;
    QLabel* m_rate10Label;
    QLabel* m_operatingTimeLabel;
    QLabel* m_utcLabel;

    void setupUi();
    int qsosInLastMinutes(int minutes) const;
};

#endif // RATEWIDGET_H
