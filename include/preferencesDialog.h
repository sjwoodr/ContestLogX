/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFontComboBox>
#include <QSpinBox>
#include <QFont>
#include <QLabel>
#include <QList>
#include <QListWidget>

#include "net/cloudStorageTypes.h"

class QrzcqApi;
class QrzApi;
class ShortcutsWidget;
class CloudStorageProvider;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog();

    bool stationChanged() const { return m_stationChanged; }
    bool themeChanged() const { return m_themeChanged; }
    bool lookupChanged() const { return m_lookupChanged; }
    bool fontsChanged() const { return m_fontsChanged; }

signals:
    // Fires on every successful Settings write - both OK and Apply.
    // MainWindow uses this to run post-save side effects (font reapply,
    // theme reapply, Remote Dashboard server restart, etc.) without
    // having to wait for the dialog to close. "Apply" keeps the dialog
    // open so the operator can iterate and see the effect (e.g. scan
    // the QR code after enabling the dashboard).
    void settingsApplied();

private slots:
    void onAccept();
    void onApply();
    void onCallsignTextChanged(const QString& text);
    void onGridTextChanged(const QString& text);
    void onStateTextChanged(const QString& text);
    void onLookupServiceChanged();
    void onTestQrzcqConnection();
    void onQrzcqSessionObtained(const QString& token);
    void onQrzcqSessionError(const QString& error);
    void onTestQrzConnection();
    void onTestOnlineScoring();
    void onQrzSessionObtained(const QString& token);
    void onQrzSessionError(const QString& error);
    void onAddDxCluster();
    void onEditDxCluster();
    void onDeleteDxCluster();

private:
    void setupUi();
    // Core save logic shared by OK and Apply. Writes every tab's fields
    // into Settings and emits settingsApplied() on success.
    void saveSettings();

    // Station tab
    QLineEdit *m_callsignEdit;
    QLineEdit *m_nameEdit;
    QLineEdit *m_gridEdit;
    QLineEdit *m_stateEdit;
    QSpinBox *m_cqZoneSpinBox;
    QSpinBox *m_ituZoneSpinBox;
    QLineEdit *m_arrlSectionEdit;
    bool m_stationChanged;

    // Shortcuts tab
    ShortcutsWidget *m_shortcutsWidget;

    // Display tab
    QComboBox *m_themeCombo;
    QCheckBox *m_forceX11Check;
    QString m_originalTheme;
    bool m_themeChanged;

    // Callsign Lookup tab
    class QRadioButton *m_lookupNoneRadio;
    class QRadioButton *m_lookupQrzcqRadio;
    class QRadioButton *m_lookupQrzRadio;
    QCheckBox *m_qrzcqAutoLookupCheckbox;
    class QGroupBox *m_qrzcqCredsGroup;
    QLineEdit *m_qrzcqUsernameEdit;
    QLineEdit *m_qrzcqPasswordEdit;
    QPushButton *m_qrzcqTestButton;
    class QGroupBox *m_qrzCredsGroup;
    QLineEdit *m_qrzUsernameEdit;
    QLineEdit *m_qrzPasswordEdit;
    QPushButton *m_qrzTestButton;
    bool m_lookupChanged;
    QrzcqApi *m_qrzcqApi;
    QrzApi   *m_qrzApi;

    // DX Cluster tab
    QListWidget *m_dxClusterList;
    QPushButton *m_dxClusterAddButton;
    QPushButton *m_dxClusterEditButton;
    QPushButton *m_dxClusterDeleteButton;

    // Fonts tab
    struct FontRow {
        QString panelKey;
        QFontComboBox *familyCombo;
        QSpinBox *sizeSpinBox;
    };
    QList<FontRow> m_fontRows;
    bool m_fontsChanged;

    // Online Scoring tab
    QCheckBox *m_osEnabledCheck;
    QLineEdit *m_osCallsignEdit;
    QLineEdit *m_osPasswordEdit;
    QComboBox *m_osIntervalCombo;
    QCheckBox *m_osPerQsoCheck;
    QPushButton *m_osTestButton;
    QLabel *m_osTestStatusLabel;
    class OnlineScoreClient *m_osTestClient;

    // Cloud Storage tab - one row per functional provider (FileLu, AWS S3)
    struct CloudProviderRow {
        CloudProviderType type;
        QCheckBox*  enabled = nullptr;
        QLineEdit*  endpoint = nullptr;
        QLineEdit*  region = nullptr;
        QLineEdit*  bucket = nullptr;
        QLineEdit*  accessKey = nullptr;
        QLineEdit*  secretKey = nullptr;
        QPushButton* testButton = nullptr;
        QLabel*     testStatus = nullptr;
    };
    QList<CloudProviderRow> m_cloudRows;
    CloudStorageProvider* m_cloudTestProvider = nullptr;
    void buildCloudProviderGroup(class QVBoxLayout* parent, CloudProviderType type,
                                 const QString& endpointDefault, const QString& regionDefault);
    void onTestCloudConnection(int rowIndex);

    // Remote Control tab
    QCheckBox  *m_rcEnabledCheck  = nullptr;
    QSpinBox   *m_rcPortSpin      = nullptr;
    QComboBox  *m_rcBindModeCombo = nullptr;
    QLineEdit  *m_rcTokenEdit     = nullptr;
    QLineEdit  *m_rcUrlLabel      = nullptr;   // read-only; kept name for minimal churn
    QPushButton *m_rcCopyUrlButton = nullptr;
    QPushButton *m_rcRotateTokenButton = nullptr;
    class QrCodeWidget *m_rcQrCode = nullptr;
    void updateRemoteControlUrlLabel();
};

#endif // PREFERENCESDIALOG_H
