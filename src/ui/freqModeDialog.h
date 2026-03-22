/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
