#ifndef CWMEMORIESDIALOG_H
#define CWMEMORIESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QList>
#include <QPushButton>
#include "cwMemory.h"

class CwMemoriesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CwMemoriesDialog(QWidget *parent = nullptr);
    ~CwMemoriesDialog();

    QList<CwMemory> getMemories() const;
    void setMemories(const QList<CwMemory>& memories);

    // Contest-specific memory support
    void setContestMemories(const QList<CwMemory>& memories);
    void setContestMode(bool contest);
    bool isContestMode() const;

private slots:
    void onSave();
    void onCancel();
    void onModeToggled();

private:
    void setupUi();
    void loadMemoriesToUi(const QList<CwMemory>& memories);
    QList<CwMemory> getMemoriesFromUi() const;

    // F1-F8 inputs
    QLineEdit *m_abbrevEdits[8];
    QTextEdit *m_textEdits[8];

    // Station/Contest radio buttons
    QRadioButton *m_stationRadio;
    QRadioButton *m_contestRadio;

    // Internal storage for both sets
    QList<CwMemory> m_stationMemories;
    QList<CwMemory> m_contestMemories;
    bool m_currentIsContest = false;
};

#endif // CWMEMORIESDIALOG_H
