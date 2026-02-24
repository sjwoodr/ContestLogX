/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 */

#ifndef SCPWIDGET_H
#define SCPWIDGET_H

#include <QDockWidget>
#include <QTableWidget>
#include <QTableWidgetItem>

class ScpWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit ScpWidget(QWidget *parent = nullptr);
    
    // Update the SCP results table
    void updateResults(const QStringList& callsigns);
    
    // Clear the table
    void clearResults();
    
    // Get the selected callsign
    QString getSelectedCallsign() const;
    
    // Set the search prefix display
    void setSearchPrefix(const QString& prefix);
    
    // Update the title based on enabled/disabled state
    void updateTitle();

signals:
    // Emitted when a callsign is selected (double-clicked)
    void callsignSelected(const QString& callsign);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onCellDoubleClicked(int row, int column);
    void layoutTable();

private:
    void setupUi();
    int getColumnCount() const;
    
    QTableWidget *m_callsignTable;
};

#endif // SCPWIDGET_H
