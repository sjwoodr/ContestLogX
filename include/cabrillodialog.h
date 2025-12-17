#ifndef CABRILLODIALOG_H
#define CABRILLODIALOG_H

#include <QDialog>
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
