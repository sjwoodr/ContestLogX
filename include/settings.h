/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QRect>
#include <QMap>
#include <QList>
#include "cwmemory.h"
#include "ssbmemory.h"

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
    
    // QRZCQ API settings
    bool getQrzcqAutoLookupEnabled() const;
    void setQrzcqAutoLookupEnabled(bool enabled);
    
    QString getQrzcqUsername() const;
    QString getQrzcqPassword() const;
    void setQrzcqCredentials(const QString& username, const QString& password);

    // Contest user prompts
    QString getContestUserPrompt(const QString& contestFile, const QString& promptId) const;
    void setContestUserPrompt(const QString& contestFile, const QString& promptId, const QString& value);
    void clearContestUserPrompts(const QString& contestFile);

    // Dialog geometries
    QByteArray getCabrilloDialogGeometry() const;
    void setCabrilloDialogGeometry(const QByteArray& geometry);

    void save();

    // Static utility function to get data directory path
    // Finds data directory relative to program invocation path (supports symlinks)
    // This is for bundled/static data files like default_layout.json, contest definitions
    static QString getDataPath();

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
