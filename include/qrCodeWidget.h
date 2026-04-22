/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * QrCodeWidget — renders a QR code for a single string (typically the
 * Remote Dashboard phone URL). Uses nayuki's header-only QR generator
 * under third_party/qrcodegen. Static image; no interaction.
 */

#ifndef QRCODEWIDGET_H
#define QRCODEWIDGET_H

#include <QString>
#include <QWidget>

class QrCodeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QrCodeWidget(QWidget* parent = nullptr);

    // Encode and render. Empty string clears the widget to a neutral
    // placeholder so the dashboard tab doesn't show a stale QR when the
    // server is disabled.
    void setData(const QString& data);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    QString m_data;
};

#endif // QRCODEWIDGET_H
