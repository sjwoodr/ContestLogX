/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "eadxDatabase.h"
#include "debugLogger.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>

EadxDatabase::EadxDatabase(QObject *parent)
    : QObject(parent)
{
}

EadxDatabase::~EadxDatabase() = default;

bool EadxDatabase::loadFromFile(const QString &filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) {
        DebugLogger::instance().log("EadxDatabase",
            QString("Failed to open %1: %2").arg(filename, f.errorString()));
        return false;
    }
    QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        DebugLogger::instance().log("EadxDatabase",
            QString("JSON parse error in %1: %2").arg(filename, err.errorString()));
        return false;
    }

    QJsonObject root = doc.object();
    QJsonArray active = root.value("active").toArray();

    m_entities.clear();
    m_prefixesByLength.clear();

    for (const QJsonValue &v : active) {
        QJsonObject obj = v.toObject();
        EadxEntity entity;
        entity.prefix    = obj.value("prefix").toString();
        entity.name      = obj.value("name").toString();
        entity.continent = obj.value("continent").toString();
        entity.cqZone    = obj.value("cqZone").toString();
        entity.ituZone   = obj.value("ituZone").toString();
        if (entity.prefix.isEmpty())
            continue;
        const QString key = entity.prefix.toUpper();
        m_entities.insert(key, entity);
        if (!m_prefixesByLength.contains(key))
            m_prefixesByLength.append(key);
    }

    // Sort prefixes longest-first so lookup picks the most specific match
    // (e.g., "EA8" before "EA", "VK9X" before "VK9" before "VK").
    std::sort(m_prefixesByLength.begin(), m_prefixesByLength.end(),
        [](const QString &a, const QString &b) {
            if (a.size() != b.size())
                return a.size() > b.size();
            return a < b;
        });

    m_loaded = !m_entities.isEmpty();
    DebugLogger::instance().log("EadxDatabase",
        QString("Loaded %1 active EADX-100 entities from %2").arg(m_entities.size()).arg(filename));
    return m_loaded;
}

QString EadxDatabase::stripPortableSuffixes(const QString &callsign) const
{
    // Strip common portable indicators that don't change DXCC entity:
    // /M (mobile), /P (portable), /MM (maritime mobile), /AM (aeronautical mobile),
    // /QRP, /QRPP, /A (alternate), digit-only suffixes (call-area shifts within
    // same entity), etc.
    static const QRegularExpression suffixRe(
        "/(M|P|MM|AM|QRP|QRPP|A|\\d)$",
        QRegularExpression::CaseInsensitiveOption);

    QString work = callsign.toUpper();
    QRegularExpressionMatch m = suffixRe.match(work);
    if (m.hasMatch())
        work = work.left(m.capturedStart());
    return work;
}

EadxEntity EadxDatabase::lookupCallsign(const QString &callsign) const
{
    if (callsign.isEmpty() || m_entities.isEmpty())
        return EadxEntity{};

    // Slash notation: "EA8/N9OH" means N9OH operating from Canary Islands —
    // the prefix BEFORE the slash names the entity. If a slash is present and
    // the part before it matches an entity prefix, use that.
    const QString upper = callsign.toUpper();
    int slashPos = upper.indexOf('/');
    if (slashPos > 0 && slashPos < upper.size() - 1) {
        const QString head = upper.left(slashPos);
        const QString tail = upper.mid(slashPos + 1);
        // Head-match form: e.g., "EA8/N9OH" → entity for "EA8"
        if (m_entities.contains(head))
            return m_entities.value(head);
        // Slash-suffix form (e.g., "JD1/O" stored verbatim as a prefix):
        // try the full callsign-as-prefix lookup against longest-first list.
        // Falls through to the general path below.
        Q_UNUSED(tail);
    }

    const QString candidate = stripPortableSuffixes(upper);

    // Longest-prefix match against the sorted list
    for (const QString &p : m_prefixesByLength) {
        if (candidate.startsWith(p))
            return m_entities.value(p);
    }
    return EadxEntity{};
}

QString EadxDatabase::getEntityPrefix(const QString &callsign) const
{
    return lookupCallsign(callsign).prefix;
}
