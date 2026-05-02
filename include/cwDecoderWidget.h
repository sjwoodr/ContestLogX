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

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QThread;
class QToolButton;
class QVBoxLayout;

class ContestEngine;

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

    // Non-owning pointer used by the Practice audio source in contest mode
    // to pull exchange format and named-mult values from the active contest.
    // Setting to nullptr (or a contest not being loaded) automatically
    // disables the "Practice - Contest Exchange" entry in the dropdown.
    void setContestEngine(ContestEngine* engine);

public slots:
    // Call after a contest is loaded or unloaded so the "Practice — Contest
    // Exchange" audio-source entry is enabled/disabled to match.
    void refreshPracticeContestAvailability();

    // Apply a font to the decoder's decoded-text rows and the labels that
    // surround them (freq / WPM / toolbar). Wired to the Preferences →
    // Fonts "CW Decoder" entry; MainWindow calls this on change.
    void setBaseFont(const QFont& font);

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
    void onStartStopClicked();
    void onCenterOrBinsChanged();
    void onSquelchChanged(int sliderValue);
    void onWordGapChanged(double multiplier);
    void onAudioDeviceChanged(int comboIndex);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void buildUi();
    void rebuildRows(const QList<double>& centerFrequencies);
    void rescanTokensForRow(int binIndex);
    void appendCharToRow(int binIndex, QChar ch);
    // Determines which row a viewport belongs to; returns -1 if not one of ours.
    int rowIndexForViewport(QObject* viewport) const;

    // OS-level microphone permission check used by beginDecoding() before
    // a real-device capture is started. Returns true if we already hold
    // permission and beginDecoding should continue immediately. Returns
    // false if permission is denied or pending — the helper itself handles
    // user feedback (modal dialog on denial) and async retry on grant
    // (re-invokes beginDecoding with the same device when the OS responds).
    // Skipped for the practice virtual sources since they don't touch a
    // real audio input device.
    bool ensureMicrophonePermissionFor(const QString& audioDeviceDescription);
    void showMicrophonePermissionDeniedDialog();

    clx::audio::RadioSide m_owningRadio;

    // Worker on its own QThread.
    clx::audio::CwDecoderWorker* m_worker = nullptr;
    QThread* m_workerThread = nullptr;

    // UI
    QVBoxLayout* m_rowsLayout = nullptr;
    QWidget* m_rowsContainer = nullptr;
    QVector<CwDecoderRow> m_rows;
    QPushButton* m_clearButton = nullptr;
    QPushButton* m_startStopButton = nullptr;
    void updateStartStopButton();
    QSpinBox* m_centerHzSpin = nullptr;
    QSpinBox* m_binCountSpin = nullptr;
    QSlider* m_squelchSlider = nullptr;
    QDoubleSpinBox* m_wordGapSpin = nullptr;
    QComboBox* m_audioDeviceCombo = nullptr;
    QLabel* m_muteIndicator = nullptr;

    bool m_muted = false;
    bool m_applyingSettings = false;

    ContestEngine* m_contestEngine = nullptr;
};

#endif // CWDECODERWIDGET_H
