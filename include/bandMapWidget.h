/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef BANDMAPWIDGET_H
#define BANDMAPWIDGET_H

#include <QDockWidget>
#include <QWidget>
#include <QHash>
#include <QDateTime>
#include <QTimer>
#include <QSlider>
#include <QLabel>
#include <functional>

// ---------------------------------------------------------------------------
// ContactStatus — classification of a spot relative to the current log
// ---------------------------------------------------------------------------
enum class ContactStatus {
    NewMultiplier,    // Working this station adds a scoring multiplier
    Worked,           // Already logged this contest session
    UnworkedNonMult,  // Not yet logged; no multiplier value
    Unknown           // Status not yet resolved (transient)
};

// ---------------------------------------------------------------------------
// SpotData — a single DX cluster spot stored by the band map
// ---------------------------------------------------------------------------
struct SpotData {
    QString callsign;
    double freqMhz = 0.0;
    QString mode;
    QString spotter;
    QDateTime timestamp;
    ContactStatus status = ContactStatus::Unknown;
};

// ---------------------------------------------------------------------------
// BandRange — frequency boundaries for the active contest band segment
// ---------------------------------------------------------------------------
struct BandRange {
    QString band;
    double minMhz = 0.0;
    double maxMhz = 0.0;
};

class BandMapCanvas;

// ---------------------------------------------------------------------------
// BandMapWidget — dockable band map panel (QDockWidget)
// ---------------------------------------------------------------------------
class BandMapWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit BandMapWidget(QWidget *parent = nullptr);
    ~BandMapWidget() override = default;

    // Compute the dedup key for a spot: "CALLSIGN|freq_rounded_to_0.1kHz"
    static QString dedupKey(const SpotData &spot);

    // Accessors (also used by unit tests)
    int    spotCount()      const { return m_spots.size(); }
    double visibleMinMhz()  const { return m_visibleMinMhz; }
    double visibleMaxMhz()  const { return m_visibleMaxMhz; }

    // Setters for configuration (also for test harness)
    void setExpirySeconds(int s) { m_expirySeconds = s; }
    void setMaxSpots(int n)      { m_maxSpots = n; }

    // Returns the status of a stored spot by callsign, or Unknown if not found.
    ContactStatus spotStatus(const QString &callsign) const;

    // Returns the timestamp of a stored spot by callsign (null QDateTime if not found).
    QDateTime spotTimestamp(const QString &callsign) const;

public slots:
    // Add a new spot or refresh an existing one if the dedup key matches.
    void addOrUpdateSpot(const SpotData &spot);

    // Set the visible band range; resets viewport to full band and clears all spots.
    void setBandRange(double minMhz, double maxMhz, const QString &band);

    // Remove all stored spots (called on new contest load or reconnect).
    void clearAllSpots();

    // Show/hide the cluster-not-connected indicator.
    void setClusterConnected(bool connected);

    // Update the rig VFO frequency line position.
    void setRigFrequency(double freqMhz);

    // For unit-test access to the expiry logic
    void onExpiryTimer();

public:
    // Re-evaluate the status of every visible spot via the supplied resolver.
    // Not a slot because MOC does not support std::function parameters.
    void refreshAllStatuses(std::function<ContactStatus(const QString&)> resolver);

signals:
    // Emitted when the operator clicks a spot. freqKhz is in kHz to match
    // the existing MainWindow::onDxSpotClicked(callsign, freq_khz, mode) signature.
    void spotClicked(const QString &callsign, double freqKhz, const QString &mode);

private slots:
    void onZoomSliderChanged(int value);

private:
    BandMapCanvas *m_canvas = nullptr;
    QSlider       *m_zoomSlider = nullptr;
    QLabel        *m_rangeLabel = nullptr;

    QHash<QString, SpotData> m_spots;
    BandRange m_bandRange;
    double m_visibleMinMhz = 0.0;
    double m_visibleMaxMhz = 0.0;

    int    m_expirySeconds   = 1800; // 30 minutes default
    int    m_maxSpots        = 30;
    bool   m_clusterConnected = true;
    double m_rigFreqMhz      = 0.0;

    QTimer *m_expiryTimer = nullptr;

    void updateRangeLabel();

    friend class BandMapCanvas;
};

// ---------------------------------------------------------------------------
// BandMapCanvas — the custom-painted viewport inside BandMapWidget
// ---------------------------------------------------------------------------
class BandMapCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit BandMapCanvas(BandMapWidget *bandMap, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    BandMapWidget *m_bandMap;

    // Pan drag state
    int    m_pressX          = -1;
    double m_dragStartVisMin = 0.0;
    double m_dragStartVisMax = 0.0;
    bool   m_dragging        = false;

    double         pixelToFreq(int x) const;
    int            freqToPixel(double freqMhz) const;
    const SpotData* spotAtPixel(int x) const;
};

#endif // BANDMAPWIDGET_H
