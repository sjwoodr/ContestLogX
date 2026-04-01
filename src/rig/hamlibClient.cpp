/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "hamlibClient.h"
#include "debugLogger.h"

// ============================================================================
// HamlibWorker — runs on background thread, owns the socket
// ============================================================================

HamlibWorker::HamlibWorker(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_connected(false)
{
}

HamlibWorker::~HamlibWorker()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
        delete m_socket;
    }
}

void HamlibWorker::doConnect(const QString& host, int port)
{
    if (m_socket) {
        m_socket->disconnectFromHost();
        delete m_socket;
    }

    m_socket = new QTcpSocket(this);
    m_socket->connectToHost(host, port);

    if (!m_socket->waitForConnected(3000)) {
        DebugLogger::instance().log("RigClient", QString("Hamlib connection failed: %1").arg(m_socket->errorString()));
        emit connectResult(false);
        return;
    }

    m_connected = true;
    DebugLogger::instance().log("RigClient", "Connected to rigctld");
    emit connectResult(true);
}

void HamlibWorker::doDisconnect()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
    m_connected = false;
    emit disconnected();
}

void HamlibWorker::doPoll()
{
    if (!m_connected || !m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // Get frequency
    QString freqResp = sendCommand("f");
    double freq = 0.0;
    if (!freqResp.isEmpty() && !freqResp.startsWith("RPRT")) {
        bool ok;
        freq = freqResp.toDouble(&ok);
        if (!ok) freq = 0.0;
    }

    // Get mode (only if freq succeeded — connection is alive)
    QString mode;
    if (freq > 0) {
        QString modeResp = sendCommand("m");
        if (!modeResp.isEmpty() && !modeResp.startsWith("RPRT")) {
            QStringList lines = modeResp.split('\n');
            mode = lines.first().trimmed();
        }
    }

    // Get CW speed
    int wpm = 0;
    if (freq > 0) {
        QString wpmResp = sendCommand("l KEYSPD");
        if (!wpmResp.isEmpty() && !wpmResp.startsWith("RPRT")) {
            bool ok;
            wpm = wpmResp.toInt(&ok);
            if (!ok) wpm = 0;
        }
    }

    // Check if socket died during polling
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        m_connected = false;
        emit socketError("Connection lost during polling");
        emit disconnected();
        return;
    }

    emit pollResult(freq, mode, wpm);
}

QString HamlibWorker::sendCommand(const QString& command, int timeoutMs)
{
    if (!m_connected || !m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return QString();
    }

    // Drain stale data
    if (m_socket->bytesAvailable() > 0) {
        m_socket->readAll();
    }

    QString cmdLine = command + "\n";
    m_socket->write(cmdLine.toUtf8());
    m_socket->flush();

    QByteArray buffer;
    int elapsed = 0;
    const int pollMs = 50;

    while (elapsed < timeoutMs) {
        if (m_socket->waitForReadyRead(pollMs)) {
            buffer.append(m_socket->readAll());
            QString partial = QString::fromUtf8(buffer);

            if (partial.contains('\n')) {
                QString trimmed = partial.trimmed();

                if (trimmed.startsWith("RPRT")) {
                    if (DebugLogger::instance().isFlrigDebugEnabled())
                        DebugLogger::instance().log("Hamlib",
                            QString("CMD '%1' -> '%2'").arg(command, trimmed));
                    return trimmed;
                }

                // Grab any additional data already available (multi-line responses)
                if (m_socket->bytesAvailable() > 0) {
                    buffer.append(m_socket->readAll());
                } else if (m_socket->waitForReadyRead(10)) {
                    buffer.append(m_socket->readAll());
                }

                QString response = QString::fromUtf8(buffer).trimmed();
                if (DebugLogger::instance().isFlrigDebugEnabled())
                    DebugLogger::instance().log("Hamlib",
                        QString("CMD '%1' -> '%2'").arg(command, response));
                return response;
            }
        }
        elapsed += pollMs;
    }

    if (DebugLogger::instance().isFlrigDebugEnabled())
        DebugLogger::instance().log("Hamlib",
            QString("CMD '%1' -> TIMEOUT (read %2 bytes)").arg(command).arg(buffer.size()));
    return QString();
}

bool HamlibWorker::isSuccess(const QString& response) const
{
    return response.trimmed() == "RPRT 0";
}

void HamlibWorker::doSetFrequency(double freqHz)
{
    QString cmd = QString("F %1").arg(static_cast<long long>(freqHz));
    emit setterResult(isSuccess(sendCommand(cmd)));
}

void HamlibWorker::doSetMode(const QString& mode)
{
    QString cmd = QString("M %1 0").arg(mode);
    emit setterResult(isSuccess(sendCommand(cmd)));
}

void HamlibWorker::doSendCW(const QString& text)
{
    QString cmd = QString("b %1").arg(text.toUpper());
    emit setterResult(isSuccess(sendCommand(cmd)));
}

void HamlibWorker::doStopCW()
{
    emit setterResult(isSuccess(sendCommand("\\stop_morse")));
}

void HamlibWorker::doSetCWSpeed(int wpm)
{
    QString cmd = QString("L KEYSPD %1").arg(wpm);
    emit setterResult(isSuccess(sendCommand(cmd)));
}

void HamlibWorker::doSetPTT(bool enable)
{
    QString cmd = QString("T %1").arg(enable ? 1 : 0);
    emit setterResult(isSuccess(sendCommand(cmd)));
}

void HamlibWorker::doSetPower(int watts)
{
    double power = watts / 100.0;
    if (power > 1.0) power = 1.0;
    if (power < 0.0) power = 0.0;
    QString cmd = QString("L RFPOWER %1").arg(power, 0, 'f', 2);
    emit setterResult(isSuccess(sendCommand(cmd)));
}

void HamlibWorker::doSetBandwidth(int hz)
{
    // Need current mode
    QString modeResp = sendCommand("m");
    if (modeResp.isEmpty() || modeResp.startsWith("RPRT")) {
        emit setterResult(false);
        return;
    }
    QString currentMode = modeResp.split('\n').first().trimmed();
    QString cmd = QString("M %1 %2").arg(currentMode).arg(hz);
    emit setterResult(isSuccess(sendCommand(cmd)));
}

void HamlibWorker::doSetVFO(const QString& vfo)
{
    QString rigVfo;
    if (vfo == "A") rigVfo = "VFOA";
    else if (vfo == "B") rigVfo = "VFOB";
    else rigVfo = vfo;
    QString cmd = QString("V %1").arg(rigVfo);
    emit setterResult(isSuccess(sendCommand(cmd)));
}

void HamlibWorker::doGetRigName()
{
    QString response = sendCommand("_");
    QString name = response.trimmed();
    m_lastRigName = (name.isEmpty() || name.startsWith("RPRT") || name == "None")
        ? "rigctld" : name;
    emit rigNameResult(m_lastRigName);
}

// ============================================================================
// HamlibClient — main thread facade, delegates to worker
// ============================================================================

HamlibClient::HamlibClient(QObject *parent)
    : RigInterface(parent)
    , m_worker(new HamlibWorker())  // no parent — will be moved to thread
    , m_cachedFreq(0.0)
    , m_cachedWpm(0)
    , m_connected(false)
{
    m_worker->moveToThread(&m_workerThread);

    // Clean up worker when thread finishes
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Worker signals -> HamlibClient slots (cross-thread, auto queued)
    connect(m_worker, &HamlibWorker::connectResult, this, [this](bool success) {
        m_connected = success;
        // Note: connected() is emitted by connectToRig() after the blocking call returns,
        // so we don't emit it here to avoid duplicate onRigConnected calls.
    });

    connect(m_worker, &HamlibWorker::disconnected, this, [this]() {
        m_connected = false;
        emit disconnected();
    });

    connect(m_worker, &HamlibWorker::socketError, this, [this](const QString& err) {
        emit error(err);
    });

    connect(m_worker, &HamlibWorker::pollResult, this, [this](double freq, const QString& mode, int wpm) {
        QMutexLocker lock(&m_mutex);
        m_cachedFreq = freq;
        if (!mode.isEmpty())
            m_cachedMode = mode;
        m_cachedWpm = wpm;
    });

    m_workerThread.start();
}

HamlibClient::~HamlibClient()
{
    m_workerThread.quit();
    m_workerThread.wait(2000);
}

bool HamlibClient::connectToRig(const QString& host, int port)
{
    DebugLogger::instance().log("RigClient", QString("Connecting to rigctld at %1:%2").arg(host).arg(port));

    QMetaObject::invokeMethod(m_worker, "doConnect", Qt::BlockingQueuedConnection,
                              Q_ARG(QString, host), Q_ARG(int, port));

    // Worker's m_connected is set by doConnect before it returns
    m_connected = m_worker->isConnected();
    if (m_connected)
        emit connected();
    return m_connected;
}

void HamlibClient::disconnectFromRig()
{
    QMetaObject::invokeMethod(m_worker, "doDisconnect", Qt::BlockingQueuedConnection);
    m_connected = false;
}

bool HamlibClient::isConnected() const
{
    return m_connected;
}

// --- Getters return cached values (updated by background polling) ---

double HamlibClient::getFrequency()
{
    // Trigger a poll on the worker thread (non-blocking)
    QMetaObject::invokeMethod(m_worker, "doPoll", Qt::QueuedConnection);

    QMutexLocker lock(&m_mutex);
    return m_cachedFreq;
}

QString HamlibClient::getMode()
{
    QMutexLocker lock(&m_mutex);
    return m_cachedMode;
}

int HamlibClient::getCWSpeed()
{
    QMutexLocker lock(&m_mutex);
    return m_cachedWpm;
}

// --- Setters fire-and-forget on worker thread ---

bool HamlibClient::setFrequency(double freqHz)
{
    QMetaObject::invokeMethod(m_worker, "doSetFrequency", Qt::QueuedConnection,
                              Q_ARG(double, freqHz));
    return true;  // optimistic
}

bool HamlibClient::setMode(const QString& mode)
{
    QMetaObject::invokeMethod(m_worker, "doSetMode", Qt::QueuedConnection,
                              Q_ARG(QString, mode));
    return true;
}

bool HamlibClient::sendCW(const QString& text)
{
    QMetaObject::invokeMethod(m_worker, "doSendCW", Qt::QueuedConnection,
                              Q_ARG(QString, text));
    return true;
}

bool HamlibClient::stopCW()
{
    QMetaObject::invokeMethod(m_worker, "doStopCW", Qt::QueuedConnection);
    return true;
}

bool HamlibClient::setCWSpeed(int wpm)
{
    QMetaObject::invokeMethod(m_worker, "doSetCWSpeed", Qt::QueuedConnection,
                              Q_ARG(int, wpm));
    return true;
}

bool HamlibClient::getPTT()
{
    return false;  // no cached PTT state from polling
}

bool HamlibClient::setPTT(bool enable)
{
    QMetaObject::invokeMethod(m_worker, "doSetPTT", Qt::QueuedConnection,
                              Q_ARG(bool, enable));
    return true;
}

int HamlibClient::getPower()
{
    return 0;  // not polled
}

bool HamlibClient::setPower(int watts)
{
    QMetaObject::invokeMethod(m_worker, "doSetPower", Qt::QueuedConnection,
                              Q_ARG(int, watts));
    return true;
}

int HamlibClient::getBandwidth()
{
    return 0;  // not polled
}

bool HamlibClient::setBandwidth(int hz)
{
    QMetaObject::invokeMethod(m_worker, "doSetBandwidth", Qt::QueuedConnection,
                              Q_ARG(int, hz));
    return true;
}

QString HamlibClient::getVFO()
{
    return QString();  // not polled
}

bool HamlibClient::setVFO(const QString& vfo)
{
    QMetaObject::invokeMethod(m_worker, "doSetVFO", Qt::QueuedConnection,
                              Q_ARG(QString, vfo));
    return true;
}

QString HamlibClient::getRigName()
{
    // Synchronous — only called from dialog, not polling
    QMetaObject::invokeMethod(m_worker, "doGetRigName", Qt::BlockingQueuedConnection);
    return m_worker->lastRigName();
}
