/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef NEXTSERIALWIDGET_H
#define NEXTSERIALWIDGET_H

#include <QWidget>

class QLabel;

// Dockable "Next S/N" panel: the serial number the operator will send on the
// next QSO, in a large font so it can be read at a glance while copying the
// other station's exchange. Shows "--" for contests without a serial.
class NextSerialWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NextSerialWidget(QWidget* parent = nullptr);

    // serial <= 0 means the current contest sends no serial number.
    void setNextSerial(int serial);
    void setBaseFont(const QFont& font);

private:
    QLabel* m_titleLabel;
    QLabel* m_valueLabel;
};

#endif // NEXTSERIALWIDGET_H
