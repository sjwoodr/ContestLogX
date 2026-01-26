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
#include <QCheckBox>

class DxClusterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DxClusterPanel(QWidget *parent = nullptr);
    ~DxClusterPanel();
    
    void loadSettings();
    void saveSettings();
    void removeSpot(const QString& callsign);

public slots:
    void setSpotCommand(const QString& callsign, double freqKhz);

signals:
    void propagationDataReceived(int sfi, int aIndex, int kIndex);
    void spotClicked(const QString& callsign, double frequency, const QString& mode);
    void spotLastQsoRequested();

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
    void onExpireSpots();
    void onSpotLastQso();

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
    QCheckBox *m_autoScrollCheckBox;
    QTcpSocket *m_socket;
    QTimer *m_propagationTimer;
    QTimer *m_expirationTimer;
    bool m_isConnected;
    bool m_loginSent;
    QString m_loginBuffer;
    QString m_callsign;
};

#endif // DXCLUSTERPANEL_H
