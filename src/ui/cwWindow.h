#ifndef CWWINDOW_H
#define CWWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QLabel>
#include <QFont>
#include "cwMemory.h"

class FlrigClient;

class CWWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CWWindow(FlrigClient* rigClient, QWidget *parent = nullptr);
    ~CWWindow();

    int getCurrentWPM() const { return wpmSpinBox->value(); }
    void setMemories(const QList<CwMemory>& memories);
    void setMemoriesFont(const QFont& font);

public:
    void sendCWText(const QString& text);

public slots:
    // Trigger a specific memory (0-7 for F1-F8)
    void onMemoryButton(int fKey);

signals:
    void wpmChanged(int wpm);
    void memoryTriggered(int fKey, const QString& text);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSendCW();
    void onClear();
    void onHalt();
    void onWpmChanged(int wpm);

private:
    
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
