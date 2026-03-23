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
    
    // Station info
    QString getCallsign() const;
    void setCallsign(const QString& call);
    
    QString getOperatorName() const;
    void setOperatorName(const QString& name);
    
    QString getGridSquare() const;
    void setGridSquare(const QString& grid);
    
    QString getState() const;
    void setState(const QString& state);

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
    
    // flrig connection
    QString getFlrigHost() const;
    void setFlrigHost(const QString& host);
    
    int getFlrigPort() const;
    void setFlrigPort(int port);
    
    int getFlrigPollInterval() const;
    void setFlrigPollInterval(int ms);
    
    bool getFlrigAutoConnect() const;
    void setFlrigAutoConnect(bool autoConnect);
    
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

    // Terms of use — version-based re-acceptance
    int getTermsAcceptedVersion() const;
    void setTermsAcceptedVersion(int version);

    // Panel fonts (keys: qsoEntry, qsoLog, dxCluster, scp, cwKeyboard)
    QFont getPanelFont(const QString& panelKey) const;
    void setPanelFont(const QString& panelKey, const QFont& font);

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
