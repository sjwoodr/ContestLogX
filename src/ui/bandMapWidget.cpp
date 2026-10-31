/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "bandMapWidget.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QSettings>
#include <QComboBox>
#include <QCheckBox>
#include <QToolButton>
#include <QFontMetrics>
#include <QtMath>

// ============================================================
// BandMapWidget
// ============================================================

BandMapWidget::BandMapWidget(QWidget *parent)
    : QDockWidget("Band Map", parent)
{
    setObjectName("BandMapWidget"); // Required for QMainWindow::saveState/restoreState

    // Load persisted settings
    QSettings s;
    m_maxAgeMinutes = s.value("BandMap/MaxAgeMinutes", 30).toInt();
    m_maxSpots      = s.value("BandMap/MaxSpots", 30).toInt();
    m_hideWorked    = s.value("BandMap/HideWorked", false).toBool();

    // ------------------------------------------------------------------
    // Content widget: toolbar row + canvas
    // ------------------------------------------------------------------
    QWidget *content = new QWidget(this);
    QVBoxLayout *vl = new QVBoxLayout(content);
    vl->setContentsMargins(2, 2, 2, 2);
    vl->setSpacing(2);

    // Toolbar row
    QHBoxLayout *hl = new QHBoxLayout();
    hl->setSpacing(4);

    QLabel *zoomLabel = new QLabel("Zoom:", content);
    hl->addWidget(zoomLabel);

    m_zoomSlider = new QSlider(Qt::Horizontal, content);
    m_zoomSlider->setRange(1, 20);
    m_zoomSlider->setValue(1); // 1 = full band
    m_zoomSlider->setMinimumWidth(60);
    m_zoomSlider->setMaximumWidth(120);
    m_zoomSlider->setToolTip("Zoom frequency axis (1=full band, 20=narrowest)");
    connect(m_zoomSlider, &QSlider::valueChanged, this, &BandMapWidget::onZoomSliderChanged);
    hl->addWidget(m_zoomSlider);

    m_rangeLabel = new QLabel("", content);
    hl->addWidget(m_rangeLabel);

    hl->addStretch();
    hl->addSpacing(12); // keeps the range label off "Age:" when the dock is narrow

    // Display filters: spot age cut-off, spot cap, hide worked
    hl->addWidget(new QLabel("Age:", content));
    m_ageCombo = new QComboBox(content);
    for (int m = 5; m <= 60; m += 5)
        m_ageCombo->addItem(QString("%1 min").arg(m), m);
    m_ageCombo->setToolTip("Hide spots older than this");
    hl->addWidget(m_ageCombo);

    hl->addWidget(new QLabel("Max:", content));
    m_maxSpotsCombo = new QComboBox(content);
    for (int n : {30, 50, 100, 200, 500})
        m_maxSpotsCombo->addItem(QString::number(n), n);
    m_maxSpotsCombo->setToolTip("Maximum spots kept (all bands); the oldest is dropped first");
    hl->addWidget(m_maxSpotsCombo);

    m_hideWorkedCheck = new QCheckBox("Hide worked", content);
    m_hideWorkedCheck->setToolTip("Hide stations already worked");
    hl->addWidget(m_hideWorkedCheck);

    // Previous / next spot (S&P)
    QToolButton *prevBtn = new QToolButton(content);
    prevBtn->setArrowType(Qt::LeftArrow);
    prevBtn->setToolTip("Tune to the next unworked spot down the band (Ctrl+Down)");
    connect(prevBtn, &QToolButton::clicked, this, [this]() { jumpToNextSpot(false); });
    hl->addWidget(prevBtn);

    QToolButton *nextBtn = new QToolButton(content);
    nextBtn->setArrowType(Qt::RightArrow);
    nextBtn->setToolTip("Tune to the next unworked spot up the band (Ctrl+Up)");
    connect(nextBtn, &QToolButton::clicked, this, [this]() { jumpToNextSpot(true); });
    hl->addWidget(nextBtn);

    vl->addLayout(hl);

    // Reflect the loaded settings, then persist changes made in the toolbar
    setMaxAgeMinutes(m_maxAgeMinutes);
    setMaxSpots(m_maxSpots);
    setHideWorked(m_hideWorked);
    connect(m_ageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        setMaxAgeMinutes(m_ageCombo->currentData().toInt());
        QSettings().setValue("BandMap/MaxAgeMinutes", m_maxAgeMinutes);
    });
    connect(m_maxSpotsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        setMaxSpots(m_maxSpotsCombo->currentData().toInt());
        QSettings().setValue("BandMap/MaxSpots", m_maxSpots);
    });
    connect(m_hideWorkedCheck, &QCheckBox::toggled, this, [this](bool checked) {
        setHideWorked(checked);
        QSettings().setValue("BandMap/HideWorked", m_hideWorked);
    });

    // Canvas (main drawing area)
    m_canvas = new BandMapCanvas(this, content);
    m_canvas->setMinimumHeight(80);
    m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_canvas->setMouseTracking(true); // needed for tooltip hover
    vl->addWidget(m_canvas);

    setWidget(content);
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable |
                QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);

    // Expiry timer - fires every 60 seconds
    m_expiryTimer = new QTimer(this);
    m_expiryTimer->setInterval(60 * 1000);
    connect(m_expiryTimer, &QTimer::timeout, this, &BandMapWidget::onExpiryTimer);
    m_expiryTimer->start();
}

// ---------------------------------------------------------------------------
// Static dedup key: "CALLSIGN|freq_rounded_to_0.1kHz"
// ---------------------------------------------------------------------------
QString BandMapWidget::dedupKey(const SpotData &spot)
{
    return spot.callsign + "|" + QString::number(qRound(spot.freqMhz * 10000));
}

// ---------------------------------------------------------------------------
// addOrUpdateSpot - add a new spot, or refresh an existing one.
// Enforces FR-016: oldest spot evicted when m_maxSpots is reached.
// ---------------------------------------------------------------------------
void BandMapWidget::addOrUpdateSpot(const SpotData &spot)
{
    if (spot.callsign.isEmpty() || spot.freqMhz <= 0.0) return;

    const QString key = dedupKey(spot);

    if (m_spots.contains(key)) {
        // Refresh existing: update timestamp + all fields, preserve the key
        SpotData &existing = m_spots[key];
        existing.timestamp = spot.timestamp;
        existing.status    = spot.status;
        existing.spotter   = spot.spotter;
        existing.mode      = spot.mode;
    } else {
        // Evict the oldest spot(s) if at capacity
        m_spots.insert(key, spot);
        evictToMaxSpots();
    }

    if (m_canvas) m_canvas->update();
}

// ---------------------------------------------------------------------------
// evictToMaxSpots - drop the oldest spots until at most m_maxSpots remain.
// ---------------------------------------------------------------------------
void BandMapWidget::evictToMaxSpots()
{
    while (m_maxSpots > 0 && m_spots.size() > m_maxSpots) {
        auto oldest = m_spots.begin();
        for (auto it = m_spots.begin(); it != m_spots.end(); ++it) {
            if (it->timestamp < oldest->timestamp) oldest = it;
        }
        m_spots.erase(oldest);
    }
}

// ---------------------------------------------------------------------------
// Display filters: max spots, age cut-off, hide worked.
// ---------------------------------------------------------------------------
void BandMapWidget::setMaxSpots(int n)
{
    m_maxSpots = n;
    if (m_maxSpotsCombo) {
        int idx = m_maxSpotsCombo->findData(n);
        if (idx >= 0) {
            QSignalBlocker block(m_maxSpotsCombo);
            m_maxSpotsCombo->setCurrentIndex(idx);
        }
    }
    evictToMaxSpots();
    if (m_canvas) m_canvas->update();
}

void BandMapWidget::setMaxAgeMinutes(int minutes)
{
    m_maxAgeMinutes = minutes;
    if (m_ageCombo) {
        int idx = m_ageCombo->findData(minutes);
        if (idx >= 0) {
            QSignalBlocker block(m_ageCombo);
            m_ageCombo->setCurrentIndex(idx);
        }
    }
    if (m_canvas) m_canvas->update();
}

void BandMapWidget::setHideWorked(bool hide)
{
    m_hideWorked = hide;
    if (m_hideWorkedCheck) {
        QSignalBlocker block(m_hideWorkedCheck);
        m_hideWorkedCheck->setChecked(hide);
    }
    if (m_canvas) m_canvas->update();
}

bool BandMapWidget::isSpotShown(const SpotData &spot) const
{
    if (m_hideWorked && spot.status == ContactStatus::Worked) return false;
    QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-qint64(m_maxAgeMinutes) * 60);
    return spot.timestamp >= cutoff;
}

// ---------------------------------------------------------------------------
// findAdjacentSpot - nearest shown, unworked spot above/below the rig
// frequency in the current band, wrapping to the far band edge if none.
// Spots within 0.2 kHz of the rig count as "here" so a repeat press moves on.
// ---------------------------------------------------------------------------
const SpotData *BandMapWidget::findAdjacentSpot(bool up) const
{
    if (m_bandRange.maxMhz <= m_bandRange.minMhz) return nullptr;
    const double tolerance = 0.0002; // 0.2 kHz

    const SpotData *nearest = nullptr; // closest in the travel direction
    const SpotData *wrap    = nullptr; // furthest the other way (band-edge wrap)
    for (auto it = m_spots.constBegin(); it != m_spots.constEnd(); ++it) {
        const SpotData &s = it.value();
        if (s.freqMhz < m_bandRange.minMhz || s.freqMhz > m_bandRange.maxMhz) continue;
        if (s.status == ContactStatus::Worked || !isSpotShown(s)) continue;

        double delta = s.freqMhz - m_rigFreqMhz;
        if (up ? delta > tolerance : delta < -tolerance) {
            if (!nearest || (up ? s.freqMhz < nearest->freqMhz : s.freqMhz > nearest->freqMhz))
                nearest = &s;
        } else if (qAbs(delta) > tolerance) {
            if (!wrap || (up ? s.freqMhz < wrap->freqMhz : s.freqMhz > wrap->freqMhz))
                wrap = &s;
        }
    }
    return nearest ? nearest : wrap;
}

// ---------------------------------------------------------------------------
// jumpToNextSpot - tune to the adjacent spot via the click-to-tune path.
// ---------------------------------------------------------------------------
bool BandMapWidget::jumpToNextSpot(bool up)
{
    const SpotData *s = findAdjacentSpot(up);
    if (!s) return false;

    // Copy before emitting: the slot may refresh statuses or add spots
    const SpotData spot = *s;

    // Move the VFO line now so a quick repeat press steps from the new
    // spot instead of waiting for the next rig poll.
    m_rigFreqMhz = spot.freqMhz;
    centerOn(spot.freqMhz);

    emit spotClicked(spot.callsign, spot.freqMhz * 1000.0, spot.mode);
    return true;
}

// ---------------------------------------------------------------------------
// centerOn - pan a zoomed view so freqMhz is visible (no-op if already shown).
// ---------------------------------------------------------------------------
void BandMapWidget::centerOn(double freqMhz)
{
    if (freqMhz >= m_visibleMinMhz && freqMhz <= m_visibleMaxMhz) {
        if (m_canvas) m_canvas->update();
        return;
    }
    double range  = m_visibleMaxMhz - m_visibleMinMhz;
    double newMin = qBound(m_bandRange.minMhz, freqMhz - range / 2.0, m_bandRange.maxMhz - range);
    m_visibleMinMhz = newMin;
    m_visibleMaxMhz = newMin + range;
    updateRangeLabel();
    if (m_canvas) m_canvas->update();
}

// ---------------------------------------------------------------------------
// setBandRange - set the visible band; resets viewport and clears all spots.
// ---------------------------------------------------------------------------
void BandMapWidget::setBandRange(double minMhz, double maxMhz, const QString &band)
{
    m_bandRange.band   = band;
    m_bandRange.minMhz = minMhz;
    m_bandRange.maxMhz = maxMhz;

    // Reset viewport to full band (zoom/pan is ephemeral per FR-015)
    m_visibleMinMhz = minMhz;
    m_visibleMaxMhz = maxMhz;

    // Spots are retained across band changes - rendering filters by visible range

    // Reset zoom slider
    if (m_zoomSlider) m_zoomSlider->setValue(1);

    updateRangeLabel();
    if (m_canvas) m_canvas->update();
}

// ---------------------------------------------------------------------------
// clearAllSpots - remove all stored spots (e.g., on cluster reconnect).
// ---------------------------------------------------------------------------
void BandMapWidget::clearAllSpots()
{
    m_spots.clear();
    if (m_canvas) m_canvas->update();
}

// ---------------------------------------------------------------------------
// refreshAllStatuses - re-evaluate contact status for every stored spot.
// ---------------------------------------------------------------------------
void BandMapWidget::refreshAllStatuses(std::function<ContactStatus(const QString&)> resolver)
{
    bool changed = false;
    for (auto it = m_spots.begin(); it != m_spots.end(); ++it) {
        ContactStatus newStatus = resolver(it->callsign);
        if (newStatus != it->status) {
            it->status = newStatus;
            changed = true;
        }
    }
    if (changed && m_canvas) m_canvas->update();
}

// ---------------------------------------------------------------------------
// setClusterConnected - show/hide the no-cluster indicator.
// ---------------------------------------------------------------------------
void BandMapWidget::setClusterConnected(bool connected)
{
    if (m_clusterConnected != connected) {
        m_clusterConnected = connected;
        if (m_canvas) m_canvas->update();
    }
}

// ---------------------------------------------------------------------------
// setRigFrequency - update the VFO line position.
// ---------------------------------------------------------------------------
void BandMapWidget::setRigFrequency(double freqMhz)
{
    if (!qFuzzyCompare(m_rigFreqMhz, freqMhz)) {
        m_rigFreqMhz = freqMhz;
        if (m_canvas) m_canvas->update();
    }
}

// ---------------------------------------------------------------------------
// onExpiryTimer - remove spots older than m_expirySeconds and repaint so
// the age filter hides spots as they cross the cut-off.
// ---------------------------------------------------------------------------
void BandMapWidget::onExpiryTimer()
{
    QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-m_expirySeconds);
    for (auto it = m_spots.begin(); it != m_spots.end(); ) {
        if (it->timestamp < cutoff)
            it = m_spots.erase(it);
        else
            ++it;
    }
    if (m_canvas) m_canvas->update();
}

// ---------------------------------------------------------------------------
// onZoomSliderChanged - map slider value (1-20) to visible frequency range.
// ---------------------------------------------------------------------------
void BandMapWidget::onZoomSliderChanged(int value)
{
    if (m_bandRange.maxMhz <= m_bandRange.minMhz) return;

    double fullWidth = m_bandRange.maxMhz - m_bandRange.minMhz;
    double newWidth  = fullWidth / static_cast<double>(value);

    // Keep centred on the current viewport midpoint
    double center = (m_visibleMinMhz + m_visibleMaxMhz) / 2.0;
    double newMin  = center - newWidth / 2.0;
    double newMax  = center + newWidth / 2.0;

    // Clamp to band edges
    if (newMin < m_bandRange.minMhz) {
        newMin = m_bandRange.minMhz;
        newMax = newMin + newWidth;
    }
    if (newMax > m_bandRange.maxMhz) {
        newMax = m_bandRange.maxMhz;
        newMin = newMax - newWidth;
        if (newMin < m_bandRange.minMhz) newMin = m_bandRange.minMhz;
    }

    // Enforce minimum zoom of 5 kHz = 0.005 MHz
    if ((newMax - newMin) < 0.005) newMax = newMin + 0.005;

    m_visibleMinMhz = newMin;
    m_visibleMaxMhz = newMax;

    updateRangeLabel();
    if (m_canvas) m_canvas->update();
}

// ---------------------------------------------------------------------------
// updateRangeLabel - show current visible frequency range in toolbar.
// ---------------------------------------------------------------------------
void BandMapWidget::updateRangeLabel()
{
    if (!m_rangeLabel) return;
    if (m_bandRange.maxMhz <= 0.0) {
        m_rangeLabel->setText("");
        return;
    }
    m_rangeLabel->setText(QString("%1 - %2 MHz")
        .arg(m_visibleMinMhz, 0, 'f', 3)
        .arg(m_visibleMaxMhz, 0, 'f', 3));
}


// ---------------------------------------------------------------------------
// spotStatus / spotTimestamp - accessors for unit tests
// ---------------------------------------------------------------------------
ContactStatus BandMapWidget::spotStatus(const QString &callsign) const
{
    for (auto it = m_spots.constBegin(); it != m_spots.constEnd(); ++it) {
        if (it->callsign == callsign) return it->status;
    }
    return ContactStatus::Unknown;
}

QDateTime BandMapWidget::spotTimestamp(const QString &callsign) const
{
    for (auto it = m_spots.constBegin(); it != m_spots.constEnd(); ++it) {
        if (it->callsign == callsign) return it->timestamp;
    }
    return QDateTime();
}

// ============================================================
// BandMapCanvas
// ============================================================

BandMapCanvas::BandMapCanvas(BandMapWidget *bandMap, QWidget *parent)
    : QWidget(parent)
    , m_bandMap(bandMap)
{
}

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------

double BandMapCanvas::pixelToFreq(int x) const
{
    if (width() <= 0) return m_bandMap->m_visibleMinMhz;
    double range = m_bandMap->m_visibleMaxMhz - m_bandMap->m_visibleMinMhz;
    return m_bandMap->m_visibleMinMhz + (static_cast<double>(x) / width()) * range;
}

int BandMapCanvas::freqToPixel(double freqMhz) const
{
    double range = m_bandMap->m_visibleMaxMhz - m_bandMap->m_visibleMinMhz;
    if (range <= 0) return 0;
    return static_cast<int>((freqMhz - m_bandMap->m_visibleMinMhz) / range * width());
}

const SpotData* BandMapCanvas::spotAtPixel(int x) const
{
    const SpotData *best = nullptr;
    int bestDist = 6; // ±5 pixel tolerance
    for (auto it = m_bandMap->m_spots.constBegin(); it != m_bandMap->m_spots.constEnd(); ++it) {
        const SpotData &s = it.value();
        if (s.freqMhz < m_bandMap->m_visibleMinMhz || s.freqMhz > m_bandMap->m_visibleMaxMhz)
            continue;
        if (!m_bandMap->isSpotShown(s)) continue;
        int sx   = freqToPixel(s.freqMhz);
        int dist = qAbs(sx - x);
        if (dist < bestDist) {
            bestDist = dist;
            best     = &it.value();
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// paintEvent
// ---------------------------------------------------------------------------
void BandMapCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QColor bg(30, 30, 30);
    p.fillRect(rect(), bg);

    const BandRange &br = m_bandMap->m_bandRange;

    // ── Empty states ────────────────────────────────────────────────────────
    if (br.maxMhz <= 0.0) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter,
                   m_bandMap->m_rigFreqMhz > 0.0 ? "Rig is outside the amateur bands"
                                                 : "Connect rig to activate band map");
        return;
    }
    // Count spots within the current band range
    bool hasVisibleSpots = false;
    for (auto it = m_bandMap->m_spots.constBegin(); it != m_bandMap->m_spots.constEnd(); ++it) {
        if (it->freqMhz >= br.minMhz && it->freqMhz <= br.maxMhz && m_bandMap->isSpotShown(*it)) {
            hasVisibleSpots = true;
            break;
        }
    }
    if (!hasVisibleSpots) {
        p.setPen(Qt::gray);
        QString msg = br.band.isEmpty()
            ? "No spots"
            : QString("No spots on %1").arg(br.band.toUpper());
        p.drawText(rect(), Qt::AlignCenter, msg);
        // Still draw axis and VFO if band is known
    }

    const int axisY  = height() - 22;
    const int markerH = 12;

    // ── Frequency axis line ─────────────────────────────────────────────────
    p.setPen(QColor(100, 100, 100));
    p.drawLine(0, axisY, width(), axisY);

    // ── Tick marks every 10 kHz = 0.01 MHz ─────────────────────────────────
    double visMin = m_bandMap->m_visibleMinMhz;
    double visMax = m_bandMap->m_visibleMaxMhz;
    double range  = visMax - visMin;

    // Choose tick interval based on visible range
    double tickInterval = 0.01; // 10 kHz default
    if (range > 0.5) tickInterval = 0.1;       // 100 kHz for wide view
    else if (range < 0.05) tickInterval = 0.001; // 1 kHz for narrow view

    QFont tickFont = font();
    tickFont.setPointSize(7);
    p.setFont(tickFont);
    p.setPen(QColor(120, 120, 120));

    double firstTick = qCeil(visMin / tickInterval) * tickInterval;
    for (double f = firstTick; f <= visMax + 1e-9; f += tickInterval) {
        int tx = freqToPixel(f);
        if (tx < 0 || tx > width()) continue;
        bool major = qFuzzyCompare(fmod(f, tickInterval * 5), 0.0) ||
                     qAbs(fmod(f, tickInterval * 5)) < 1e-9;
        int tickH = major ? 8 : 4;
        p.drawLine(tx, axisY, tx, axisY + tickH);
        if (major) {
            // Label in kHz offset from band start (or absolute MHz for wider views)
            QString label;
            if (range > 0.1)
                label = QString::number(f, 'f', 3);
            else
                label = QString("+%1k").arg(qRound((f - br.minMhz) * 1000));
            p.drawText(tx - 20, axisY + 10, 40, 12, Qt::AlignHCenter, label);
        }
    }

    // ── Spot markers ────────────────────────────────────────────────────────
    QFont labelFont = font();
    labelFont.setPointSize(8);
    labelFont.setBold(true);
    p.setFont(labelFont);

    const int markerW = 6;
    const int labelY  = axisY - markerH - 2;

    for (auto it = m_bandMap->m_spots.constBegin(); it != m_bandMap->m_spots.constEnd(); ++it) {
        const SpotData &s = it.value();
        if (s.freqMhz < visMin || s.freqMhz > visMax) continue;
        if (!m_bandMap->isSpotShown(s)) continue;

        int sx = freqToPixel(s.freqMhz);

        // Marker fill color
        QColor fill;
        switch (s.status) {
        case ContactStatus::NewMultiplier:   fill = QColor("#FF6B00"); break;
        case ContactStatus::Worked:          fill = QColor("#505050"); break;
        case ContactStatus::UnworkedNonMult: fill = QColor("#1E90FF"); break;
        default:                             fill = QColor("#808080"); break;
        }

        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRect(sx - markerW / 2, axisY - markerH, markerW, markerH);

        // Callsign label (truncated if needed)
        p.setPen(Qt::white);
        QFontMetrics fm(labelFont);
        QString label = s.callsign;
        int labelW = fm.horizontalAdvance(label);
        int lx = sx - labelW / 2;
        // Clamp to widget bounds
        lx = qMax(0, qMin(lx, width() - labelW));
        p.drawText(lx, labelY, label);
    }

    // ── VFO line ────────────────────────────────────────────────────────────
    if (m_bandMap->m_rigFreqMhz >= visMin && m_bandMap->m_rigFreqMhz <= visMax) {
        int vx = freqToPixel(m_bandMap->m_rigFreqMhz);
        p.setPen(QPen(QColor("#00FF00"), 1));
        p.drawLine(vx, 0, vx, axisY);
    }

    // ── No cluster indicator ─────────────────────────────────────────────────
    if (!m_bandMap->m_clusterConnected) {
        p.setPen(QColor("#FF4444"));
        QFont indFont = font();
        indFont.setPointSize(8);
        p.setFont(indFont);
        p.drawText(rect().adjusted(2, 2, -2, -2),
                   Qt::AlignTop | Qt::AlignRight, "No cluster");
    }
}

// ---------------------------------------------------------------------------
// mousePressEvent - record press position for drag/click disambiguation.
// ---------------------------------------------------------------------------
void BandMapCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressX          = event->position().toPoint().x();
        m_dragStartVisMin = m_bandMap->m_visibleMinMhz;
        m_dragStartVisMax = m_bandMap->m_visibleMaxMhz;
        m_dragging        = false;
    }
    QWidget::mousePressEvent(event);
}

// ---------------------------------------------------------------------------
// mouseMoveEvent - pan drag + tooltip.
// ---------------------------------------------------------------------------
void BandMapCanvas::mouseMoveEvent(QMouseEvent *event)
{
    int cx = event->position().toPoint().x();

    // Pan drag: left button held
    if ((event->buttons() & Qt::LeftButton) && m_pressX >= 0) {
        int dx = cx - m_pressX;
        if (qAbs(dx) > 3) m_dragging = true;

        if (m_dragging) {
            double range = m_dragStartVisMax - m_dragStartVisMin;
            if (range > 0 && width() > 0) {
                double mhzPerPixel = range / width();
                double offset = -dx * mhzPerPixel;
                double newMin = m_dragStartVisMin + offset;
                double newMax = m_dragStartVisMax + offset;

                // Clamp to band edges
                if (newMin < m_bandMap->m_bandRange.minMhz) {
                    newMin = m_bandMap->m_bandRange.minMhz;
                    newMax = newMin + range;
                }
                if (newMax > m_bandMap->m_bandRange.maxMhz) {
                    newMax = m_bandMap->m_bandRange.maxMhz;
                    newMin = newMax - range;
                    if (newMin < m_bandMap->m_bandRange.minMhz)
                        newMin = m_bandMap->m_bandRange.minMhz;
                }

                m_bandMap->m_visibleMinMhz = newMin;
                m_bandMap->m_visibleMaxMhz = newMax;
                m_bandMap->updateRangeLabel();
                update();
            }
        }
    }

    // Tooltip: find nearest spot
    const SpotData *s = spotAtPixel(cx);
    if (s) {
        qint64 ageSeconds = s->timestamp.secsTo(QDateTime::currentDateTimeUtc());
        int ageMin = static_cast<int>(ageSeconds / 60);
        QString tip = QString("%1\n%2 MHz · %3\nSpotter: %4\nAge: %5 min")
            .arg(s->callsign)
            .arg(s->freqMhz, 0, 'f', 3)
            .arg(s->mode)
            .arg(s->spotter)
            .arg(ageMin);
        QToolTip::showText(event->globalPosition().toPoint(), tip, this);
    } else {
        QToolTip::hideText();
    }

    QWidget::mouseMoveEvent(event);
}

// ---------------------------------------------------------------------------
// mouseReleaseEvent - emit spotClicked if this was a click (not a drag).
// ---------------------------------------------------------------------------
void BandMapCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_dragging && m_pressX >= 0) {
        const SpotData *s = spotAtPixel(m_pressX);
        if (s) {
            // Emit freq in kHz to match MainWindow::onDxSpotClicked signature
            emit m_bandMap->spotClicked(s->callsign, s->freqMhz * 1000.0, s->mode);
        }
    }
    m_pressX   = -1;
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

// ---------------------------------------------------------------------------
// wheelEvent - zoom in/out centred on the cursor frequency.
// ---------------------------------------------------------------------------
void BandMapCanvas::wheelEvent(QWheelEvent *event)
{
    const BandRange &br = m_bandMap->m_bandRange;
    if (br.maxMhz <= br.minMhz) { event->ignore(); return; }

    double center = pixelToFreq(event->position().toPoint().x());
    double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25; // up=zoom-in
    double range  = (m_bandMap->m_visibleMaxMhz - m_bandMap->m_visibleMinMhz) * factor;

    // Enforce minimum zoom (5 kHz)
    if (range < 0.005) range = 0.005;

    double newMin = center - range / 2.0;
    double newMax = center + range / 2.0;

    // Clamp to band edges
    if (newMin < br.minMhz) { newMin = br.minMhz; newMax = newMin + range; }
    if (newMax > br.maxMhz) { newMax = br.maxMhz; newMin = newMax - range; }
    if (newMin < br.minMhz)   newMin = br.minMhz;

    m_bandMap->m_visibleMinMhz = newMin;
    m_bandMap->m_visibleMaxMhz = newMax;

    // Sync slider to approximate zoom level
    if (m_bandMap->m_zoomSlider) {
        double fullWidth = br.maxMhz - br.minMhz;
        double newRange  = newMax - newMin;
        int sliderVal = (newRange > 0)
            ? qBound(1, qRound(fullWidth / newRange), 20)
            : 1;
        m_bandMap->m_zoomSlider->blockSignals(true);
        m_bandMap->m_zoomSlider->setValue(sliderVal);
        m_bandMap->m_zoomSlider->blockSignals(false);
    }

    m_bandMap->updateRangeLabel();
    update();
    event->accept();
}
