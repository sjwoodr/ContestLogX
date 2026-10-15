/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef CABRILLODIALOG_H
#define CABRILLODIALOG_H

#include <QDialog>
#include <QMap>
#include <QJsonObject>

class QLineEdit;
class QComboBox;
class QTextEdit;
class QLabel;

class CabrilloDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CabrilloDialog(QWidget *parent = nullptr);
    
    QJsonObject getHeaderData() const;
    void setCallsign(const QString& call);
    void setClaimedScore(int score);
    void loadFromSettings();
    // Pre-fill from contest setup (CabrilloDefaults::fromSetup). Category
    // dropdowns take the setup value; headers the dialog has no field for
    // (IOTA-ISLAND-NAME, CATEGORY-LOCATION, ...) get an editable row each.
    void applySetupDefaults(const QMap<QString, QString>& headers);
    
    void closeEvent(QCloseEvent *event) override;

private:
    QLineEdit* m_callsignEdit;
    QLineEdit* m_operatorEdit;
    QComboBox* m_categoryCombo;
    QComboBox* m_categoryPowerCombo;
    QComboBox* m_categoryModeCombo;
    QComboBox* m_categoryOperatorCombo;
    QComboBox* m_categoryBandCombo;
    QComboBox* m_categoryTransmitterCombo;
    QComboBox* m_categoryAssistedCombo;
    QComboBox* m_categoryOverlayCombo;
    QComboBox* m_categoryStationCombo;
    class QVBoxLayout* m_extraHeadersLayout = nullptr;
    QMap<QString, QLineEdit*> m_extraHeaderEdits;  // header name -> field
    QLineEdit* m_addressCityEdit;
    QLineEdit* m_locationEdit;
    QLineEdit* m_clubEdit;
    QLineEdit* m_nameEdit;
    QLineEdit* m_addressEdit;
    QLineEdit* m_stateEdit;
    QLineEdit* m_postalEdit;
    QLineEdit* m_countryEdit;
    QLineEdit* m_emailEdit;
    QTextEdit* m_commentsEdit;
    QLabel* m_claimedScoreLabel;
    
    void setupUI();
    void saveToSettings() const;
};

#endif // CABRILLODIALOG_H
