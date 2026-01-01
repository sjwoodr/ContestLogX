/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef SCPLINEEDIT_H
#define SCPLINEEDIT_H

#include <QLineEdit>

class ScpWidget;

class ScpLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit ScpLineEdit(QWidget *parent = nullptr);
    
    // Set the SCP widget to update when search changes
    void setScpWidget(ScpWidget *widget);
    
    // Enable/disable SCP search
    void setScpEnabled(bool enabled);
    bool isScpEnabled() const { return m_scpEnabled; }

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void performScpSearch();
    void onScpCallsignSelected(const QString& callsign);

private:
    void updateScpWidget();
    
    ScpWidget *m_scpWidget;
    bool m_scpEnabled;
};

#endif // SCPLINEEDIT_H

