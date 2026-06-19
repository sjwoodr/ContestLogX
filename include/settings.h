/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QRect>
#include <QMap>
#include <QList>
#include <QFont>
#include "cwMemory.h"
#include "ssbMemory.h"

/**
 * @brief Application settings manager
 * 
 * Uses JSON file format for cross-platform settings storage
 */
class Settings
{
public:
    static Settings& instance();

    // Override the directory where Settings reads/writes ContestLogX.json.
    // Must be called BEFORE the first Settings::instance() call (ideally
    // very early in main() right after CLI parsing). Used by the
    // --config-dir flag to sandbox a CLX session — local smoke testing,
    // running multiple CLX instances with different identities, etc.
    // Empty string clears the override (default location is used).
    static void setOverrideConfigDir(const QString& dir);
    static QString overrideConfigDir();

    // Station info
    QString getCallsign() const;
    void setCallsign(const QString& call);
    
    QString getOperatorName() const;
    void setOperatorName(const QString& name);
    
    QString getGridSquare() const;
    void setGridSquare(const QString& grid);
    
    QString getState() const;
    void setState(const QString& state);

    QString getDxccCountry() const;
    void setDxccCountry(const QString& prefix);

    int getCqZone() const;
    void setCqZone(int zone);

    int getItuZone() const;
    void setItuZone(int zone);

    QString getArrlSection() const;
    void setArrlSection(const QString& section);

    QString getCabrilloEmail() const;
    void setCabrilloEmail(const QString& email);
    
    QString getCabrilloAddressCity() const;
    void setCabrilloAddressCity(const QString& city);
    
    QString getCabrilloAddress() const;
    void setCabrilloAddress(const QString& address);
    
    QString getCabrilloPostalCode() const;
    void setCabrilloPostalCode(const QString& code);
    
    QString getCabrilloCountry() const;
    void setCabrilloCountry(const QString& country);
    
    QString getCabrilloClub() const;
    void setCabrilloClub(const QString& club);
    
    QString getCabrillSoapbox() const;
    void setCabrillSoapbox(const QString& soapbox);
    
    QString getCabrilloCategory() const;
    void setCabrilloCategory(const QString& category);
    
    QString getCabrilloPower() const;
    void setCabrilloPower(const QString& power);
    
    QString getCabrilloMode() const;
    void setCabrilloMode(const QString& mode);
    
    QString getCabrilloOperatorType() const;
    void setCabrilloOperatorType(const QString& opType);
    
    QString getCabrilloBand() const;
    void setCabrilloBand(const QString& band);
    
    QString getCabrilloTransmitter() const;
    void setCabrilloTransmitter(const QString& transmitter);
    
    QString getCabrilloAssisted() const;
    void setCabrilloAssisted(const QString& assisted);
    
    QString getCabrilloOverlay() const;
    void setCabrilloOverlay(const QString& overlay);
    
    // Rig connection - backend selection
    QString getRigBackend() const;       // "flrig" or "hamlib"
    void setRigBackend(const QString& backend);

    // flrig connection
    QString getFlrigHost() const;
    void setFlrigHost(const QString& host);

    int getFlrigPort() const;
    void setFlrigPort(int port);

    int getFlrigPollInterval() const;
    void setFlrigPollInterval(int ms);

    bool getFlrigAutoConnect() const;
    void setFlrigAutoConnect(bool autoConnect);

    // Hamlib (rigctld) connection
    QString getHamlibHost() const;
    void setHamlibHost(const QString& host);

    int getHamlibPort() const;
    void setHamlibPort(int port);

    bool getHamlibAutoConnect() const;
    void setHamlibAutoConnect(bool autoConnect);

    // Mocked rig connection
    bool getMockedAutoConnect() const;
    void setMockedAutoConnect(bool autoConnect);

    // SO2R settings
    bool getSo2rEnabled() const;
    void setSo2rEnabled(bool enabled);

    // Radio R rig connection - backend selection
    QString getRadioRRigBackend() const;
    void setRadioRRigBackend(const QString& backend);

    // Radio R flrig connection
    QString getRadioRFlrigHost() const;
    void setRadioRFlrigHost(const QString& host);
    int getRadioRFlrigPort() const;
    void setRadioRFlrigPort(int port);
    bool getRadioRFlrigAutoConnect() const;
    void setRadioRFlrigAutoConnect(bool autoConnect);

    // Radio R Hamlib (rigctld) connection
    QString getRadioRHamlibHost() const;
    void setRadioRHamlibHost(const QString& host);
    int getRadioRHamlibPort() const;
    void setRadioRHamlibPort(int port);
    bool getRadioRHamlibAutoConnect() const;
    void setRadioRHamlibAutoConnect(bool autoConnect);

    // Radio R mocked rig connection
    bool getRadioRMockedAutoConnect() const;
    void setRadioRMockedAutoConnect(bool autoConnect);

    // CW Decoder — per-radio audio input + PTT mute settings (SPEC-005)
    QString getRadioLAudioInputDevice() const;
    void setRadioLAudioInputDevice(const QString& deviceDescription);
    bool getRadioLMuteDecoderOnPtt() const;
    void setRadioLMuteDecoderOnPtt(bool enabled);
    int getRadioLDecoderPttGraceMs() const;
    void setRadioLDecoderPttGraceMs(int ms);

    QString getRadioRAudioInputDevice() const;
    void setRadioRAudioInputDevice(const QString& deviceDescription);
    bool getRadioRMuteDecoderOnPtt() const;
    void setRadioRMuteDecoderOnPtt(bool enabled);
    int getRadioRDecoderPttGraceMs() const;
    void setRadioRDecoderPttGraceMs(int ms);

    // CW Decoder runtime settings (per-radio: right=true selects Radio R)
    QJsonObject getCwDecoderSettings(bool right) const;
    void setCwDecoderSettings(bool right, const QJsonObject& obj);
    int getCwDecoderPassbandLowHz(bool right) const;
    void setCwDecoderPassbandLowHz(bool right, int hz);
    int getCwDecoderPassbandHighHz(bool right) const;
    void setCwDecoderPassbandHighHz(bool right, int hz);
    // Center frequency — derived passband = center ± (bins × 25). Replaces
    // the two-spinbox passbandLow/High controls in the widget UI. If an
    // existing profile has passbandLow/High but no centerHz, the getter
    // migrates to (low+high)/2.
    int getCwDecoderCenterHz(bool right) const;
    void setCwDecoderCenterHz(bool right, int hz);
    int getCwDecoderBinCount(bool right) const;
    void setCwDecoderBinCount(bool right, int n);
    double getCwDecoderSquelch(bool right) const;
    void setCwDecoderSquelch(bool right, double threshold);
    int getCwDecoderWpmMin(bool right) const;
    void setCwDecoderWpmMin(bool right, int wpm);
    int getCwDecoderWpmMax(bool right) const;
    void setCwDecoderWpmMax(bool right, int wpm);
    double getCwDecoderWordGap(bool right) const;
    void setCwDecoderWordGap(bool right, double multiplier);
    bool getCwDecoderSquelchAuto(bool right) const;
    void setCwDecoderSquelchAuto(bool right, bool enabled);

    // CW keyer (WinKeyer) per-radio settings: cwKeyer.left / cwKeyer.right.
    // source is "rig" (key via the rig backend, default) or "winkeyer".
    QString getCwKeyerSource(bool right) const;
    void setCwKeyerSource(bool right, const QString& source);
    QString getCwKeyerPort(bool right) const;
    void setCwKeyerPort(bool right, const QString& port);

    // WSJT-X integration
    bool getWsjtxEnabled() const;
    void setWsjtxEnabled(bool enabled);
    int getWsjtxPort() const;
    void setWsjtxPort(int port);

    // CW settings
    int getCwWpm() const;
    void setCwWpm(int wpm);

    QList<CwMemory> getCwMemories() const;
    void setCwMemories(const QList<CwMemory>& memories);

    int getCwSnPadding() const;         // Minimum digits for {SN}: 1, 2, or 3 (default 1)
    void setCwSnPadding(int digits);
    bool getCwSnCutNumbers() const;     // Send cut numbers for {SN} (default false)
    void setCwSnCutNumbers(bool enabled);

    // SSB settings
    QList<SsbMemory> getSsbMemories() const;
    void setSsbMemories(const QList<SsbMemory>& memories);
    
    // Window geometry
    QRect getWindowGeometry() const;
    void setWindowGeometry(const QRect& geometry);

    QByteArray getWindowGeometryState() const;
    void setWindowGeometryState(const QByteArray& state);

    bool getWindowMaximized() const;
    void setWindowMaximized(bool maximized);
    
    // Column widths
    QMap<int, int> getColumnWidths() const;
    void setColumnWidth(int column, int width);
    
    // Splitter sizes
    QList<int> getMainSplitterSizes() const;
    void setMainSplitterSizes(const QList<int>& sizes);
    
    QList<int> getRightPanelSplitterSizes() const;
    void setRightPanelSplitterSizes(const QList<int>& sizes);
    
    // DX Cluster settings
    QString getDxClusterServer() const;
    void setDxClusterServer(const QString& server);

    QStringList getDxClusterServers() const;
    void setDxClusterServers(const QStringList& servers);

    QString getDxClusterCallsign() const;
    void setDxClusterCallsign(const QString& callsign);
    
    // Panel visibility
    bool getDxClusterVisible() const;
    void setDxClusterVisible(bool visible);
    
    bool getCwConsoleVisible() const;
    void setCwConsoleVisible(bool visible);
    
    // Dock widget state
    QByteArray getDockWidgetState() const;
    void setDockWidgetState(const QByteArray& state);
    
    // Debug settings
    bool getFlrigDebugEnabled() const;
    void setFlrigDebugEnabled(bool enabled);
    bool getMainWindowDebugEnabled() const;
    void setMainWindowDebugEnabled(bool enabled);
    bool getContestEngineDebugEnabled() const;
    void setContestEngineDebugEnabled(bool enabled);
    bool getContestSelectDialogDebugEnabled() const;
    void setContestSelectDialogDebugEnabled(bool enabled);
    bool getCWWindowDebugEnabled() const;
    void setCWWindowDebugEnabled(bool enabled);
    
    bool getDxccDatabaseDebugEnabled() const;
    void setDxccDatabaseDebugEnabled(bool enabled);
    
    bool getDxClusterDebugEnabled() const;
    void setDxClusterDebugEnabled(bool enabled);

    bool getScpDebugEnabled() const;
    void setScpDebugEnabled(bool enabled);

    bool getMultiplierWidgetDebugEnabled() const;
    void setMultiplierWidgetDebugEnabled(bool enabled);

    bool getCallsignLookupDebugEnabled() const;
    void setCallsignLookupDebugEnabled(bool enabled);

    bool getWsjtxDebugEnabled() const;
    void setWsjtxDebugEnabled(bool enabled);

    bool getCwDecoderDebugEnabled() const;
    void setCwDecoderDebugEnabled(bool enabled);
    bool getWinKeyerDebugEnabled() const;
    void setWinKeyerDebugEnabled(bool enabled);

    // Splitter states (as byte arrays)
    QByteArray getMainSplitterState() const;
    void setMainSplitterState(const QByteArray& state);
    
    QByteArray getRightPanelSplitterState() const;
    void setRightPanelSplitterState(const QByteArray& state);
    
    // Keyboard shortcuts
    QMap<QString, QString> getShortcuts() const;
    void setShortcuts(const QMap<QString, QString>& shortcuts);
    QString getShortcut(const QString& actionName) const;
    void setShortcut(const QString& actionName, const QString& keySequence);
    
    // Call history settings
    bool getCallHistoryEnabled() const;
    void setCallHistoryEnabled(bool enabled);
    
    bool getCallHistoryAutoSaveEnabled() const;
    void setCallHistoryAutoSaveEnabled(bool enabled);
    
    // Super Check Partial (SCP) settings
    bool getScpEnabled() const;
    void setScpEnabled(bool enabled);
    
    // Callsign lookup service: "qrzcq", "qrz", or "none"
    QString getCallsignLookupService() const;
    void setCallsignLookupService(const QString& service);

    // QRZCQ API settings
    bool getQrzcqAutoLookupEnabled() const;
    void setQrzcqAutoLookupEnabled(bool enabled);

    QString getQrzcqUsername() const;
    QString getQrzcqPassword() const;
    void setQrzcqCredentials(const QString& username, const QString& password);

    // QRZ.com API settings
    QString getQrzUsername() const;
    QString getQrzPassword() const;
    void setQrzCredentials(const QString& username, const QString& password);

    // Contest user prompts
    QString getContestUserPrompt(const QString& contestFile, const QString& promptId) const;
    void setContestUserPrompt(const QString& contestFile, const QString& promptId, const QString& value);
    void clearContestUserPrompts(const QString& contestFile);

    // Dialog geometries
    QByteArray getCabrilloDialogGeometry() const;
    void setCabrilloDialogGeometry(const QByteArray& geometry);

    // SSB voice keying settings
    bool getSsbKeyingEnabled() const;
    void setSsbKeyingEnabled(bool enabled);
    QString getTtsCommand() const;
    void setTtsCommand(const QString& command);
    QString getTtsArgs() const;
    void setTtsArgs(const QString& args);
    QString getAudioPlayCommand() const;
    void setAudioPlayCommand(const QString& command);
    QString getAudioPlayArgs() const;
    void setAudioPlayArgs(const QString& args);

    // Online scoring
    bool getOnlineScoringEnabled() const;
    void setOnlineScoringEnabled(bool enabled);
    QString getOnlineScoringCallsign() const;
    QString getOnlineScoringPassword() const;
    void setOnlineScoringCredentials(const QString& callsign, const QString& password);
    int getOnlineScoringInterval() const;
    void setOnlineScoringInterval(int minutes);
    bool getOnlineScoringPerQso() const;
    void setOnlineScoringPerQso(bool perQso);

    // Theme
    QString getTheme() const;
    void setTheme(const QString& theme);

    // Force X11/XCB backend (for Wayland window position restore)
    bool getForceX11() const;
    void setForceX11(bool enabled);

    // Terms of use — version-based re-acceptance
    int getTermsAcceptedVersion() const;
    void setTermsAcceptedVersion(int version);

    // Panel fonts (keys: qsoEntry, qsoLog, dxCluster, scp, cwKeyboard)
    QFont getPanelFont(const QString& panelKey) const;
    void setPanelFont(const QString& panelKey, const QFont& font);

    // Remote Control — embedded HTTP server for LAN dashboards and minimal
    // rig control from mobile devices (TODO item 3). Off by default; when
    // enabled, binds according to getRemoteControlBindMode() and serves
    // read-only JSON endpoints plus a static dashboard page at /.
    bool getRemoteControlEnabled() const;
    void setRemoteControlEnabled(bool enabled);
    int  getRemoteControlPort() const;       // default 8080
    void setRemoteControlPort(int port);
    // Bind mode: "lan" = first non-loopback IPv4 interface, "localhost" =
    // 127.0.0.1 only, "any" = 0.0.0.0 (useful when LAN autodetect picks
    // the wrong interface, e.g. VPN tunnels present).
    QString getRemoteControlBindMode() const;
    void    setRemoteControlBindMode(const QString& mode);
    // Shared bearer token (also accepted as ?token= query param). Auto-
    // generated on first enable; user can rotate via Preferences.
    QString getRemoteControlToken() const;
    void    setRemoteControlToken(const QString& token);

    void save();

    // Static utility function to get data directory path
    // Finds data directory relative to program invocation path (supports symlinks)
    // This is for bundled/static data files like default_layout.json, contest definitions
    static QString getDataPath();

    // Static utility function to get contests directory path
    // Finds contests directory, with AppImage support via APPDIR env var
    static QString getContestsPath();

    // Static utility function to get user data directory path
    // Returns AppDataLocation for user-writable data files (master.scp, cty.dat, history.json)
    static QString getUserDataPath();

private:
    Settings();
    void load();
    void scaleDefaultLayout();

    QString settingsFilePath() const;
    
    QJsonObject m_settings;
    bool m_modified;
};

#endif // SETTINGS_H
