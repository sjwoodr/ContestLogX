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

#include "rateWidget.h"
#include "qsoListModel.h"
#include "qsoRecord.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QDateTime>
#include <QFont>

RateWidget::RateWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &RateWidget::onRefresh);
    m_timer->start(1000);  // refresh every second
}

void RateWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    QGridLayout* grid = new QGridLayout;
    grid->setSpacing(4);

    auto makeValueLabel = [this]() {
        QLabel* l = new QLabel("—", this);
        QFont f = l->font();
        f.setBold(true);
        l->setFont(f);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };

    auto makeCaptionLabel = [this](const QString& text) {
        QLabel* l = new QLabel(text, this);
        l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return l;
    };

    int row = 0;

    grid->addWidget(makeCaptionLabel("Rate (60 min):"),  row, 0);
    m_rate60Label = makeValueLabel();
    grid->addWidget(m_rate60Label, row++, 1);

    grid->addWidget(makeCaptionLabel("Rate (10 min):"),  row, 0);
    m_rate10Label = makeValueLabel();
    grid->addWidget(m_rate10Label, row++, 1);

    grid->addWidget(makeCaptionLabel("Operating time:"), row, 0);
    m_operatingTimeLabel = makeValueLabel();
    grid->addWidget(m_operatingTimeLabel, row++, 1);

    grid->addWidget(makeCaptionLabel("UTC:"),            row, 0);
    m_utcLabel = makeValueLabel();
    grid->addWidget(m_utcLabel, row++, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    mainLayout->addLayout(grid);
    mainLayout->addStretch();

    onRefresh();
}

void RateWidget::setModel(const QsoListModel* model)
{
    m_model = model;
    onRefresh();
}

void RateWidget::setBaseFont(const QFont& font)
{
    setFont(font);
    for (QLabel* l : findChildren<QLabel*>())
        l->setFont(font);
    // Re-apply bold to value labels
    QFont bold = font;
    bold.setBold(true);
    for (QLabel* l : {m_rate60Label, m_rate10Label, m_operatingTimeLabel, m_utcLabel})
        l->setFont(bold);
}

int RateWidget::qsosInLastMinutes(int minutes) const
{
    if (!m_model) return 0;
    QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-60 * minutes);
    int count = 0;
    for (const QsoRecord& qso : m_model->getQsos()) {
        if (qso.getDateTime() >= cutoff)
            ++count;
    }
    return count;
}

void RateWidget::onRefresh()
{
    // UTC clock — update every tick
    m_utcLabel->setText(QDateTime::currentDateTimeUtc().toString("HH:mm:ss") + " Z");

    if (!m_model || m_model->count() == 0) {
        m_rate60Label->setText("—");
        m_rate10Label->setText("—");
        m_operatingTimeLabel->setText("—");
        return;
    }

    // Rates expressed as QSOs/hour
    int q60 = qsosInLastMinutes(60);
    int q10 = qsosInLastMinutes(10);
    m_rate60Label->setText(QString::number(q60) + "/hr");
    m_rate10Label->setText(QString::number(q10 * 6) + "/hr");

    // Operating time: elapsed since the first QSO's timestamp
    QDateTime first = m_model->getQsos().first().getDateTime();
    if (first.isValid()) {
        qint64 secs = first.secsTo(QDateTime::currentDateTimeUtc());
        if (secs < 0) secs = 0;
        int h = secs / 3600;
        int m = (secs % 3600) / 60;
        int s = secs % 60;
        m_operatingTimeLabel->setText(QString("%1:%2:%3")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0')));
    } else {
        m_operatingTimeLabel->setText("—");
    }
}
