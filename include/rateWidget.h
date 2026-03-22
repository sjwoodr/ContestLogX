/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
