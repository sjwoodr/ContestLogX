#ifndef DXCLUSTERPANEL_H
#define DXCLUSTERPANEL_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTcpSocket>
#include <QComboBox>
#include <QTextEdit>
#include <QDialog>

class DxClusterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DxClusterPanel(QWidget *parent = nullptr);
    ~DxClusterPanel();
    
    void loadSettings();
    void saveSettings();

signals:
    void propagationDataReceived(int sfi, int aIndex, int kIndex);
    void spotClicked(const QString& callsign, double frequency, const QString& mode);

private slots:
    void onSpotClicked(int row, int column);
    void onConnect();
    void onDisconnect();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError();
    void onViewChanged(int index);
    void onSendCommand();
    void onPropagationTimerTimeout();

private:
    void setupUi();
    void addSpot(const QString& callsign, double frequency, const QString& spotter, const QString& comment);
    void showLoginDialog();
    void sendLoginAndCommands();
    
    QTableWidget *m_spotTable;
    QTextEdit *m_consoleText;
    QLineEdit *m_clusterEdit;
    QLineEdit *m_commandEdit;
    QPushButton *m_connectButton;
    QComboBox *m_viewCombo;
    QTcpSocket *m_socket;
    QTimer *m_propagationTimer;
    bool m_isConnected;
    bool m_loginSent;
    QString m_loginBuffer;
    QString m_callsign;
};

#endif // DXCLUSTERPANEL_H
