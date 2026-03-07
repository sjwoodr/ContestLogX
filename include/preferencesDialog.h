/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
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
#include <QList>
#include <QListWidget>

class QrzcqApi;
class QrzApi;
class ShortcutsWidget;

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

private slots:
    void onAccept();
    void onCallsignTextChanged(const QString& text);
    void onGridTextChanged(const QString& text);
    void onStateTextChanged(const QString& text);
    void onLookupServiceChanged();
    void onTestQrzcqConnection();
    void onQrzcqSessionObtained(const QString& token);
    void onQrzcqSessionError(const QString& error);
    void onTestQrzConnection();
    void onQrzSessionObtained(const QString& token);
    void onQrzSessionError(const QString& error);
    void onAddDxCluster();
    void onEditDxCluster();
    void onDeleteDxCluster();

private:
    void setupUi();

    // Station tab
    QLineEdit *m_callsignEdit;
    QLineEdit *m_nameEdit;
    QLineEdit *m_gridEdit;
    QLineEdit *m_stateEdit;
    bool m_stationChanged;

    // Shortcuts tab
    ShortcutsWidget *m_shortcutsWidget;

    // Display tab
    QComboBox *m_themeCombo;
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
};

#endif // PREFERENCESDIALOG_H
