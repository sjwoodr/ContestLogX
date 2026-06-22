/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "winKeyerClient.h"
#include "debugLogger.h"

#include <QSerialPort>
#include <QThread>

// WinKey host protocol (WK2/WK3) constants
namespace {
    const char WK_ADMIN          = 0x00;
    const char WK_ADMIN_HOSTOPEN = 0x02;
    const char WK_ADMIN_HOSTCLOSE= 0x03;
    const char WK_CMD_SETWPM     = 0x02;
    const char WK_CMD_CLEAR      = 0x0A;  // clear buffer / abort send

    // A WinKey status byte has its top two bits set; bit 2 is the BUSY flag.
    inline bool isStatusByte(unsigned char b) { return (b & 0xC0) == 0xC0; }
    inline bool statusBusy(unsigned char b)   { return (b & 0x04) != 0; }

    const int WK_BAUD = 1200;

    // ESP32-based clones (e.g. AtomKey) auto-reset when the port opens; give
    // the device time to boot before the handshake. Harmless for a real K1EL.
    const int WK_BOOT_MS      = 1600;
    const int WK_HANDSHAKE_MS = 1000;
}

// ============================================================================
// WinKeyerWorker - runs on background thread, owns the serial port
// ============================================================================

WinKeyerWorker::WinKeyerWorker(QObject *parent)
    : QObject(parent)
    , m_serial(nullptr)
    , m_connected(false)
    , m_revision(-1)
    , m_lastBusy(false)
{
}

WinKeyerWorker::~WinKeyerWorker()
{
    if (m_serial) {
        m_serial->close();
        delete m_serial;
    }
}

void WinKeyerWorker::doOpen(const QString& portName)
{
    if (m_serial) {
        m_serial->close();
        delete m_serial;
        m_serial = nullptr;
    }
    m_connected = false;
    m_revision = -1;
    m_lastBusy = false;

    m_serial = new QSerialPort(this);
    m_serial->setPortName(portName);
    m_serial->setBaudRate(WK_BAUD);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit serialError(QString("Cannot open %1: %2").arg(portName, m_serial->errorString()));
        delete m_serial;
        m_serial = nullptr;
        emit openResult(false, -1);
        return;
    }

    QThread::msleep(WK_BOOT_MS);
    m_serial->clear();

    // Host-open: admin(0x00) + open(0x02) -> device returns a revision byte.
    const char hostOpen[2] = { WK_ADMIN, WK_ADMIN_HOSTOPEN };
    m_serial->write(hostOpen, 2);
    m_serial->flush();

    int rev = -1;
    if (m_serial->waitForReadyRead(WK_HANDSHAKE_MS)) {
        QByteArray resp = m_serial->readAll();
        if (!resp.isEmpty())
            rev = static_cast<unsigned char>(resp.at(0));
    }

    if (rev < 0) {
        emit serialError("No response to WinKey host-open (wrong port, or not a WinKeyer?)");
        m_serial->close();
        delete m_serial;
        m_serial = nullptr;
        emit openResult(false, -1);
        return;
    }

    m_revision = rev;
    m_connected = true;
    connect(m_serial, &QSerialPort::readyRead, this, &WinKeyerWorker::onReadyRead);
    DebugLogger::instance().log("WinKeyer",
        QString("Host-open OK on %1, revision 0x%2").arg(portName).arg(rev, 2, 16, QChar('0')));
    emit openResult(true, rev);
}

void WinKeyerWorker::doClose()
{
    if (m_serial) {
        if (m_connected) {
            const char hostClose[2] = { WK_ADMIN, WK_ADMIN_HOSTCLOSE };
            m_serial->write(hostClose, 2);
            m_serial->flush();
            m_serial->waitForBytesWritten(200);
        }
        m_serial->close();
        delete m_serial;
        m_serial = nullptr;
    }
    m_connected = false;
    emit closed();
}

void WinKeyerWorker::doSendCW(const QString& text)
{
    if (!m_connected || !m_serial) {
        DebugLogger::instance().log("WinKeyer",
            QString("doSendCW SKIPPED (connected=%1 serial=%2): %3")
                .arg(m_connected).arg(m_serial != nullptr).arg(text));
        return;
    }
    // Printable ASCII is keyed as Morse; uppercase to match contest convention.
    QByteArray bytes = text.toUpper().toLatin1();
    const qint64 n = m_serial->write(bytes);
    const bool flushed = m_serial->flush();
    DebugLogger::instance().log("WinKeyer",
        QString("doSendCW wrote %1/%2 bytes (flush=%3): %4")
            .arg(n).arg(bytes.size()).arg(flushed).arg(QString::fromLatin1(bytes)));
}

void WinKeyerWorker::doStopCW()
{
    if (!m_connected || !m_serial)
        return;
    m_serial->write(&WK_CMD_CLEAR, 1);
    m_serial->flush();
}

void WinKeyerWorker::doSetCWSpeed(int wpm)
{
    if (!m_connected || !m_serial)
        return;
    if (wpm < 5)  wpm = 5;
    if (wpm > 99) wpm = 99;
    const char cmd[2] = { WK_CMD_SETWPM, static_cast<char>(wpm) };
    m_serial->write(cmd, 2);
    m_serial->flush();
}

void WinKeyerWorker::onReadyRead()
{
    if (!m_serial)
        return;
    const QByteArray data = m_serial->readAll();
    for (char ch : data) {
        const unsigned char b = static_cast<unsigned char>(ch);
        if (isStatusByte(b)) {
            const bool busy = statusBusy(b);
            if (busy != m_lastBusy) {
                m_lastBusy = busy;
                emit busyChanged(busy);
            }
        }
        // Echo bytes and other status bits are ignored for now (see TODO M3).
    }
}

// ============================================================================
// WinKeyerClient - main thread facade, delegates to worker
// ============================================================================

WinKeyerClient::WinKeyerClient(QObject *parent)
    : QObject(parent)
    , m_worker(new WinKeyerWorker())  // no parent - moved to thread
    , m_connected(false)
    , m_revision(-1)
    , m_cachedWpm(0)
{
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Async open completion (connectAsync path): update state and notify.
    connect(m_worker, &WinKeyerWorker::openResult, this, [this](bool ok, int rev) {
        {
            QMutexLocker lock(&m_mutex);
            m_connected = ok;
            m_revision = rev;
            if (!ok) m_portName.clear();
        }
        if (ok) emit connected();
    });

    connect(m_worker, &WinKeyerWorker::closed, this, [this]() {
        m_connected = false;
        emit disconnected();
    });

    connect(m_worker, &WinKeyerWorker::serialError, this, [this](const QString& err) {
        emit error(err);
    });

    connect(m_worker, &WinKeyerWorker::busyChanged, this, [this](bool busy) {
        emit busyChanged(busy);
    });

    m_workerThread.start();
}

WinKeyerClient::~WinKeyerClient()
{
    m_workerThread.quit();
    m_workerThread.wait(2000);
}

bool WinKeyerClient::openPort(const QString& portName)
{
    DebugLogger::instance().log("WinKeyer", QString("Opening keyer on %1").arg(portName));

    QMetaObject::invokeMethod(m_worker, "doOpen", Qt::BlockingQueuedConnection,
                              Q_ARG(QString, portName));

    // Worker has finished doOpen by the time the blocking call returns.
    {
        QMutexLocker lock(&m_mutex);
        m_connected = m_worker->isConnected();
        m_revision = m_worker->revision();
        m_portName = m_connected ? portName : QString();
    }
    if (m_connected)
        emit connected();
    return m_connected;
}

void WinKeyerClient::connectAsync(const QString& portName)
{
    {
        QMutexLocker lock(&m_mutex);
        m_portName = portName;
    }
    DebugLogger::instance().log("WinKeyer", QString("Opening keyer (async) on %1").arg(portName));
    // Non-blocking: doOpen (incl. the boot-wait) runs on the worker thread;
    // state + connected() are delivered via the openResult handler.
    QMetaObject::invokeMethod(m_worker, "doOpen", Qt::QueuedConnection,
                              Q_ARG(QString, portName));
}

void WinKeyerClient::closePort()
{
    QMetaObject::invokeMethod(m_worker, "doClose", Qt::BlockingQueuedConnection);
    QMutexLocker lock(&m_mutex);
    m_connected = false;
    m_portName.clear();
}

bool WinKeyerClient::isConnected() const
{
    return m_connected;
}

bool WinKeyerClient::sendCW(const QString& text)
{
    QMetaObject::invokeMethod(m_worker, "doSendCW", Qt::QueuedConnection,
                              Q_ARG(QString, text));
    return true;
}

bool WinKeyerClient::stopCW()
{
    QMetaObject::invokeMethod(m_worker, "doStopCW", Qt::QueuedConnection);
    return true;
}

int WinKeyerClient::getCWSpeed()
{
    QMutexLocker lock(&m_mutex);
    return m_cachedWpm;
}

bool WinKeyerClient::setCWSpeed(int wpm)
{
    {
        QMutexLocker lock(&m_mutex);
        m_cachedWpm = wpm;
    }
    QMetaObject::invokeMethod(m_worker, "doSetCWSpeed", Qt::QueuedConnection,
                              Q_ARG(int, wpm));
    return true;
}

int WinKeyerClient::revision() const
{
    QMutexLocker lock(&m_mutex);
    return m_revision;
}

QString WinKeyerClient::portName() const
{
    QMutexLocker lock(&m_mutex);
    return m_portName;
}
