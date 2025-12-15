#ifndef CWWINDOW_H
#define CWWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QLabel>
#include "cwmemory.h"

class FlrigClient;

class CWWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CWWindow(FlrigClient* rigClient, QWidget *parent = nullptr);
    ~CWWindow();
    
    int getCurrentWPM() const { return wpmSpinBox->value(); }
    void setMemories(const QList<CwMemory>& memories);

signals:
    void wpmChanged(int wpm);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSendCW();
    void onClear();
    void onHalt();
    void onWpmChanged(int wpm);
    void onMemoryButton(int fKey);

private:
    void sendCWText(const QString& text);
    
    QTextEdit* historyText;
    QLineEdit* inputLine;
    QPushButton* clearButton;
    QPushButton* haltButton;
    QSpinBox* wpmSpinBox;
    QPushButton* memoryButtons[8];  // F1-F8
    FlrigClient* rigClient;
    QList<CwMemory> memories;
};

#endif // CWWINDOW_H
