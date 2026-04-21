# Contracts: Qt Signal Interfaces

**Phase 1 output** · 2026-04-21 · Feature branch `004-cw-decoder`

This document defines the signal/slot interfaces introduced by SPEC-005. These are the stable contracts between the decoder subsystem and the rest of the application; tests will exercise them directly and the widget/MainWindow wiring depends on their shapes.

---

## Naming conventions

- Signal names use camelCase, past tense for events (`charDecoded`), present tense for state (`muteForInternalSend`).
- `QString` for textual payloads, `int` for bin indices and counts, `bool` for binary state, `qint64` for monotonic timestamps in ms.
- All signals are Qt signals; all slots are either Qt slots or plain member functions invoked by `QMetaObject::invokeMethod(..., Qt::QueuedConnection, ...)` for cross-thread invocation.

---

## 1. `RigInterface` (existing, extended)

**Location**: `include/rigInterface.h`

**Addition**:

```cpp
signals:
    void pttStateChanged(bool active);   // NEW — SPEC-005 R4
```

**Emission contract**:
- MUST be emitted on every observed PTT state transition. Multiple emissions with the same value (e.g., `false` → `false`) are tolerated by consumers but SHOULD be avoided.
- MUST be emitted on the rig backend's thread context (main thread for `FlrigClient`, worker thread for `HamlibClient`). Consumers receive it on the main thread via `Qt::AutoConnection` (direct within-thread, queued across).
- MAY lag the physical radio by up to the backend's polling interval (≤ 500 ms typical for flrig). This lag is documented and acceptable per the edge-case entry in the spec.

**Backwards compatibility**: No existing consumer subscribes to this signal; adding it does not affect other code.

---

## 2. `AudioCapture` (new)

**Location**: `src/audio/audioCapture.cpp`, `include/audio/audioCapture.h`

**Purpose**: Thin wrapper around `QAudioSource` that owns the device handle and forwards audio blocks into an SPSC ring buffer.

**Construction**: `AudioCapture(const QAudioDevice& device, QObject* parent = nullptr)`. The device handle is resolved by `RigControlDialog` from the persisted `deviceDescription` via `QMediaDevices::audioInputs()`.

**Public API**:

```cpp
class AudioCapture : public QObject {
    Q_OBJECT
public:
    explicit AudioCapture(const QAudioDevice& device, QObject* parent = nullptr);
    ~AudioCapture();

    bool start();     // returns true on success; emits deviceError on failure
    void stop();
    bool isRunning() const;

signals:
    void audioBlockReady(qint64 captureTimestampMs);  // emitted when ring buffer has a new 80-sample block
    void deviceError(const QString& message);          // emitted on format negotiation failure or device disappearance
    void captureStarted();
    void captureStopped();
};
```

**Ring buffer access**: The ring buffer is a member of `AudioCapture` exposed via a non-virtual `popBlock(int16_t out[80])` method. The decoder worker calls it on the worker thread after receiving an `audioBlockReady` signal (or independently polls).

**Threading**:
- Construction and `start()`/`stop()` called on the main thread.
- The `QAudioSource::readyRead` handler runs on the main thread's event loop; drains samples into the ring buffer, emits `audioBlockReady` via `Qt::QueuedConnection` to the worker.
- `popBlock()` is thread-safe: producer on main, consumer on worker (SPSC).

---

## 3. `CwDecoderWorker` (new)

**Location**: `src/audio/cwDecoderWorker.cpp`, `include/audio/cwDecoderWorker.h`

**Purpose**: `QObject` lifecycle-managed on a dedicated `QThread`. Owns the `CwDecoder` instance and relays decoded events to the widget.

**Construction**: `CwDecoderWorker(QObject* parent = nullptr)`. The worker is moved to a `QThread` immediately after construction by its owner (`DecoderSession`).

**Slots (invoked by cross-thread calls from `DecoderSession`)**:

```cpp
public slots:
    void startCapture(AudioCapture* capture,
                      int passbandLowHz, int passbandHighHz,
                      int binCount,
                      int wpmMin, int wpmMax,
                      float squelchThreshold);

    void stopCapture();

    // FR-018: restart DSP within 1 second
    void reconfigure(int passbandLowHz, int passbandHighHz, int binCount);

    // Runtime-adjustable settings (no DSP restart needed)
    void setWpmRange(int wpmMin, int wpmMax);
    void setSquelch(float threshold);

    // Mute control (FR-019a, FR-019c)
    void setPttMute(bool active);                     // path 1 — rig backend
    void muteForInternalSend(int durationMs);         // path 2 — MainWindow signals
```

**Signals (emitted toward the widget on the main thread via `Qt::QueuedConnection`)**:

```cpp
signals:
    void charDecoded(int binIndex, QChar ch, qint64 timestampMs);
    void wpmUpdated(int binIndex, int wpm);                    // 0 = no lock
    void toneActiveChanged(int binIndex, bool active);         // for optional visual indicator
    void binLayoutChanged(const QList<double>& centerFrequencies);  // after reconfigure; widget relabels rows
    void muteStateChanged(bool muted);                         // UI hint (e.g., dim all rows)
    void errorOccurred(const QString& message);
```

**Emission rate limits**:
- `charDecoded` emits at the actual decode rate (~8 chars/sec per bin at 40 WPM, up to ~16 bins total → 128 events/sec peak). Below Qt's signal overhead.
- `wpmUpdated` emits only on *change* of the integer WPM value per bin.
- `toneActiveChanged` emits only on transitions.

---

## 4. `CwDecoderWidget` (new)

**Location**: `src/ui/cwDecoderWidget.cpp`, `include/cwDecoderWidget.h`

**Purpose**: The `QDockWidget` UI. Receives decoded events from its `CwDecoderWorker` and renders the N-row scrolling display.

**Construction**: `CwDecoderWidget(RadioSide owningRadio, QWidget* parent = nullptr)`. The radio binding is immutable for the widget's lifetime.

**Public API**:

```cpp
class CwDecoderWidget : public QDockWidget {
    Q_OBJECT
public:
    explicit CwDecoderWidget(RadioSide owningRadio, QWidget* parent = nullptr);
    ~CwDecoderWidget();

    RadioSide owningRadio() const;

    // Settings load/save (called by MainWindow on construction and on close)
    void loadSettings();
    void saveSettings();

    // Observers for tests
    int binCount() const;
    int currentWpm(int binIndex) const;  // 0 if no lock
    QString bufferText(int binIndex) const;

signals:
    // Emitted when operator clicks a clickable token.
    // NOTE: binIndex is informational for UI/tests only; routing is by owningRadio (FR-023).
    void callClicked(const QString& callsign, int binIndex);
    void rstClicked(const QString& rst, int binIndex);
};
```

**Click-fill contract** (critical — FR-020, FR-021, FR-022, FR-023, FR-024a):
- Widget emits `callClicked(callsign, binIndex)` and `rstClicked(rst, binIndex)`.
- `MainWindow` connects these signals to handlers that target the widget's `owningRadio`'s entry panel.
- The handlers MUST invoke the **same** code path used for keyboard entry of the same characters. In current code, keyboard entry sets the field's text and triggers the field's `textEdited` signal handler (which runs SCP, call-history, dupe check, etc.). The click-fill handler MUST mirror this exactly — do NOT call any internal method that keyboard entry bypasses.
- The handler MUST NOT call `setFocus()` on any widget (FR-022).

---

## 5. `MainWindow` (existing, extended)

**Location**: `src/ui/mainWindow.cpp`, `include/mainWindow.h`

### New members

```cpp
private:
    CwDecoderWidget* m_cwDecoderLeft = nullptr;   // nullptr if Radio L has no audio configured
    CwDecoderWidget* m_cwDecoderRight = nullptr;  // nullptr if Radio R has no audio configured
```

### New slots

```cpp
private slots:
    // Called when an RigControlDialog apply changes the audio device for a radio
    void onAudioDeviceChanged(RadioSide side);

    // Called when the operator clicks a token in a decoder widget
    void onDecoderCallClicked(const QString& callsign, int binIndex);
    void onDecoderRstClicked(const QString& rst, int binIndex);

    // Called when the rig backend for a specific radio reports PTT state
    void onRigPttChanged(RadioSide side, bool active);
```

### New signal (outbound to decoders)

None — internal-send mute is invoked by direct method call on the decoder widget / worker:

```cpp
void MainWindow::notifyInternalCwSend(RadioSide side, int textChars, int sendWpm) {
    CwDecoderWidget* w = (side == RadioSide::Left) ? m_cwDecoderLeft : m_cwDecoderRight;
    if (!w) return;
    int durMs = static_cast<int>(std::ceil(textChars * 60.0 / (sendWpm * 5.0) * 1000))
              + Settings::decoderPttGraceMs(side);
    w->muteForInternalSend(durMs);
}
```

This method MUST be called from every existing CW-send code path — F-key memory handler, CW console send button, and any other internal path that invokes `flrigClient->cwioText(...)` or equivalent. Implementation audits all existing send sites as a task.

### PTT wiring (both radios)

On `RigInterface` construction for each radio, connect:

```cpp
connect(m_rigLeft, &RigInterface::pttStateChanged,
        this, [this](bool active){ onRigPttChanged(RadioSide::Left, active); });
connect(m_rigRight, &RigInterface::pttStateChanged,
        this, [this](bool active){ onRigPttChanged(RadioSide::Right, active); });
```

`onRigPttChanged` forwards to the corresponding decoder widget's `setPttMute(active)`, respecting the `muteDecoderOnPtt` setting — if the setting is false for that radio, the slot does nothing.

---

## 6. `CwDecoder` (new, internal-only API)

Not exposed via Qt signals — all DSP happens inside the worker. Documented here for completeness.

```cpp
class CwDecoder {
public:
    CwDecoder(int sampleRate);   // 8000 by default
    ~CwDecoder();

    void configure(int passbandLowHz, int passbandHighHz, int binCount,
                   int wpmMin, int wpmMax, float squelchThreshold);
    void setMuted(bool muted);

    // Called by worker per audio block
    // Returns decoded events for emission
    struct DecodeEvent {
        int binIndex;
        enum { Char, WpmChange, ToneTransition } kind;
        QChar character;    // valid if kind == Char
        int wpm;            // valid if kind == WpmChange
        bool toneActive;    // valid if kind == ToneTransition
    };
    QList<DecodeEvent> processBlock(const int16_t* samples, int sampleCount, qint64 timestampMs);

    // Observers (for tests)
    int binCount() const;
    double binCenterFreq(int binIndex) const;
};
```

Pure C++, no Qt types in the signature other than `QChar` / `QList` (for compatibility with the Qt signal path). Fully testable without audio hardware using synthetic `int16_t` input.

---

## 7. Settings keys (new)

All persisted via `QSettings` (no JSON files).

**Rig per-radio keys** (under `rig.left.*` and `rig.right.*`):

| Key | Type | Default | Notes |
|---|---|---|---|
| `audioInputDevice` | `QString` | `""` | Empty = "(none)" |
| `muteDecoderOnPtt` | `bool` | `true` | |
| `decoderPttGraceMs` | `int` | `250` | Range 0–2000 |

**Decoder runtime keys** (under `audio/cwDecoder/left/*` and `audio/cwDecoder/right/*`):

| Key | Type | Default | Notes |
|---|---|---|---|
| `passbandLowHz` | `int` | `400` | |
| `passbandHighHz` | `int` | `1000` | |
| `binCount` | `int` | `6` | Range 1–16 |
| `spotlightRowIndex` | `int` | `-1` | -1 = none |
| `squelchThreshold` | `float` | `0.05` | Range 0.0–1.0 |
| `wpmMin` | `int` | `5` | |
| `wpmMax` | `int` | `60` | |

---

## Testability

Every contract above can be exercised without audio hardware:

- `RigInterface::pttStateChanged`: mocked by `MockedRigClient::emit pttStateChanged(true/false)` in unit tests.
- `AudioCapture`: in unit tests, substitute a test double that directly posts pre-recorded `int16_t` blocks into the ring buffer without `QAudioSource`.
- `CwDecoderWorker` + `CwDecoder`: call `processBlock` with synthetic sine samples (pure C++ test, no Qt dependencies).
- `CwDecoderWidget`: create the widget headless (no dock parent), programmatically call `muteForInternalSend`, and assert on `bufferText(binIndex)` + `callClicked` signal via `QSignalSpy`.
- `MainWindow` integration: use `MockedRigClient` for both radios, synthesize PTT transitions, and verify the decoder's `muteStateChanged` signal fires correctly.

---

## Summary table

| Contract | Direction | Purpose |
|---|---|---|
| `RigInterface::pttStateChanged(bool)` | Rig backend → MainWindow → Decoder | FR-019a rig-backend PTT mute |
| `AudioCapture::audioBlockReady(qint64)` | Capture → Worker | Signal a new 80-sample block is in the ring |
| `CwDecoderWorker::charDecoded(binIdx, ch, ts)` | Worker → Widget | Per-bin decoded character |
| `CwDecoderWorker::wpmUpdated(binIdx, wpm)` | Worker → Widget | Per-bin live WPM |
| `CwDecoderWorker::binLayoutChanged(freqs)` | Worker → Widget | Post-reconfigure row labels |
| `CwDecoderWidget::callClicked(call, binIdx)` | Widget → MainWindow | FR-020 CALL click-fill |
| `CwDecoderWidget::rstClicked(rst, binIdx)` | Widget → MainWindow | FR-021 RSTr click-fill |
| `CwDecoderWorker::setPttMute(bool)` | MainWindow slot | FR-019a apply rig-backend mute |
| `CwDecoderWorker::muteForInternalSend(int)` | MainWindow → Worker | FR-019c internal-send mute |
| `MainWindow::notifyInternalCwSend(...)` | Internal CW send sites | FR-019c trigger point |
