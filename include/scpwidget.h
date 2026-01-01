/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef SCPWIDGET_H
#define SCPWIDGET_H

#include <QDockWidget>
#include <QListWidget>

class ScpWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit ScpWidget(QWidget *parent = nullptr);
    
    // Update the SCP results list
    void updateResults(const QStringList& callsigns);
    
    // Clear the list
    void clearResults();
    
    // Get the selected callsign
    QString getSelectedCallsign() const;
    
    // Set the search prefix display
    void setSearchPrefix(const QString& prefix);

signals:
    // Emitted when a callsign is selected (double-clicked)
    void callsignSelected(const QString& callsign);

private slots:
    void onCallsignDoubleClicked(QListWidgetItem* item);

private:
    void setupUi();
    
    QListWidget *m_callsignList;
};

#endif // SCPWIDGET_H
