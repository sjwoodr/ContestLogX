/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * CwDecoderWidget — dockable multi-channel CW decoder panel (SPEC-005).
 * One widget per radio bound to a specific RadioSide at construction time.
 */

#ifndef CWDECODERWIDGET_H
#define CWDECODERWIDGET_H

#include <QDockWidget>
#include <QChar>
#include <QList>
#include <QString>
#include <QVector>

#include "audio/audioTypes.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QThread;
class QToolButton;
class QVBoxLayout;

namespace clx::audio {
class CwDecoderWorker;
class AudioCapture;
}

struct CwDecoderRow {
    QLabel* freqLabel = nullptr;
    QLabel* wpmLabel = nullptr;
    QPlainTextEdit* text = nullptr;
    QWidget* container = nullptr;
};

class CwDecoderWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit CwDecoderWidget(clx::audio::RadioSide owningRadio, QWidget* parent = nullptr);
    ~CwDecoderWidget() override;

    clx::audio::RadioSide owningRadio() const { return m_owningRadio; }
    bool isRightRadio() const { return m_owningRadio == clx::audio::RadioSide::Right; }

    // Called by MainWindow when the operator has configured an audio device
    // for this radio. Starts capture + worker.
    void beginDecoding(const QString& audioDeviceDescription);
    void endDecoding();

    // Forwarded to the worker (via queued connection).
    void muteForInternalSend(int durationMs);
    void setPttMute(bool active);

    // Observers (useful for tests + CHK).
    int currentWpm(int binIndex) const;
    QString bufferText(int binIndex) const;

    void loadSettings();
    void saveSettings();

signals:
    // Emitted when the operator clicks a decoded CALL token. binIndex is
    // informational — MainWindow routes to owningRadio's entry.
    void callClicked(const QString& callsign, int binIndex);
    void rstClicked(const QString& rst, int binIndex);

private slots:
    void onCharDecoded(int binIndex, QChar ch, qint64 timestampMs);
    void onWpmUpdated(int binIndex, int wpm);
    void onBinLayoutChanged(const QList<double>& centerFrequencies);
    void onMuteStateChanged(bool muted);
    void onClearClicked();
    void onPassbandChanged();
    void onBinCountChanged();
    void onSquelchChanged(int sliderValue);
    void onWpmRangeChanged();
    void onSpotlightRowChanged(int rowIndex);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void buildUi();
    void rebuildRows(const QList<double>& centerFrequencies);
    void applySpotlightVisuals();
    void rescanTokensForRow(int binIndex);
    void appendCharToRow(int binIndex, QChar ch);
    // Determines which row a viewport belongs to; returns -1 if not one of ours.
    int rowIndexForViewport(QObject* viewport) const;

    clx::audio::RadioSide m_owningRadio;

    // Worker on its own QThread.
    clx::audio::CwDecoderWorker* m_worker = nullptr;
    QThread* m_workerThread = nullptr;

    // UI
    QVBoxLayout* m_rowsLayout = nullptr;
    QWidget* m_rowsContainer = nullptr;
    QVector<CwDecoderRow> m_rows;
    QPushButton* m_clearButton = nullptr;
    QSpinBox* m_passbandLowSpin = nullptr;
    QSpinBox* m_passbandHighSpin = nullptr;
    QSpinBox* m_binCountSpin = nullptr;
    QSlider* m_squelchSlider = nullptr;
    QSpinBox* m_wpmMinSpin = nullptr;
    QSpinBox* m_wpmMaxSpin = nullptr;
    QSpinBox* m_spotlightSpin = nullptr;
    QLabel* m_muteIndicator = nullptr;

    bool m_muted = false;
    int m_spotlightRow = -1;
    bool m_applyingSettings = false;
};

#endif // CWDECODERWIDGET_H
