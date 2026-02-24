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

#ifndef FREQMODEDIALOG_H
#define FREQMODEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>

class FreqModeDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit FreqModeDialog(QWidget *parent = nullptr);
    
    void setFrequency(double freqKhz);
    void setMode(const QString& mode);
    
    double frequency() const;
    QString mode() const;
    
private slots:
    void onOkClicked();
    void onCancelClicked();
    
private:
    void setupUi();
    
    QLineEdit* m_freqEdit;
    QComboBox* m_modeCombo;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    
    double m_frequency;
    QString m_mode;
};

#endif // FREQMODEDIALOG_H
