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
