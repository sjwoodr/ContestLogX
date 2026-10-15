/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "nextSerialWidget.h"

#include <QLabel>
#include <QVBoxLayout>

NextSerialWidget::NextSerialWidget(QWidget* parent)
    : QWidget(parent)
    , m_titleLabel(new QLabel("Next S/N", this))
    , m_valueLabel(new QLabel("--", this))
{
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setAlignment(Qt::AlignCenter);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_valueLabel, 1);

    setBaseFont(font());
}

void NextSerialWidget::setNextSerial(int serial)
{
    if (serial > 0) {
        m_valueLabel->setText(QString::number(serial));
        m_valueLabel->setToolTip("Serial number you will send on the next QSO");
    } else {
        m_valueLabel->setText("--");
        m_valueLabel->setToolTip("This contest does not use serial numbers");
    }
}

void NextSerialWidget::setBaseFont(const QFont& font)
{
    QFont titleFont = font;
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    // Big enough to read at a glance from the operating position.
    QFont valueFont = font;
    valueFont.setBold(true);
    valueFont.setPointSize(qMax(24, font.pointSize() * 4));
    m_valueLabel->setFont(valueFont);
}
