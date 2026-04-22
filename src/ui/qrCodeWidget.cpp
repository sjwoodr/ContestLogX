/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "qrCodeWidget.h"
#include "qrcodegen/qrcodegen.hpp"

#include <QPainter>

namespace {
constexpr int kPreferredPx = 180;   // compact enough for a Preferences row
constexpr int kQuietModules = 2;    // white border around the code (spec says 4, 2 is fine for LAN use)
} // namespace

QrCodeWidget::QrCodeWidget(QWidget* parent)
    : QWidget(parent)
{
    // Fixed square footprint; phones focus better on a QR that isn't
    // stretched. Qt will still lay it out in the form, just not resize.
    setFixedSize(kPreferredPx, kPreferredPx);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void QrCodeWidget::setData(const QString& data)
{
    if (m_data == data) return;
    m_data = data;
    update();
}

QSize QrCodeWidget::sizeHint() const
{
    return QSize(kPreferredPx, kPreferredPx);
}

void QrCodeWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);

    if (m_data.isEmpty()) {
        p.setPen(QColor("#8b949e"));
        p.drawText(rect(), Qt::AlignCenter, tr("(disabled)"));
        return;
    }

    try {
        // MEDIUM error-correction trades ~15% capacity for ~15% damage
        // tolerance — good default for printed/photographed QR codes, more
        // than enough for a scanned-from-screen URL.
        const auto qr = qrcodegen::QrCode::encodeText(
            m_data.toUtf8().constData(),
            qrcodegen::QrCode::Ecc::MEDIUM);
        const int size = qr.getSize();
        const int totalModules = size + 2 * kQuietModules;
        const int pxPerModule = std::max(1,
            std::min(width(), height()) / totalModules);
        const int qrPx = pxPerModule * totalModules;
        const int offsetX = (width()  - qrPx) / 2;
        const int offsetY = (height() - qrPx) / 2;

        // White quiet zone is already set via fillRect above; just draw
        // the dark modules.
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (!qr.getModule(x, y)) continue;
                p.drawRect(offsetX + (x + kQuietModules) * pxPerModule,
                           offsetY + (y + kQuietModules) * pxPerModule,
                           pxPerModule, pxPerModule);
            }
        }
    } catch (const std::exception&) {
        // Overlong text or internal encoder error — show a readable
        // placeholder instead of crashing the Preferences dialog.
        p.setPen(QColor("#c0392b"));
        p.drawText(rect(), Qt::AlignCenter, tr("(encoder error)"));
    }
}
