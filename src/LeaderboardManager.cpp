#include "LeaderboardManager.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QDir>
#include <QPair>
#include <QSet>
#include <algorithm>

LeaderboardManager::LeaderboardManager(QObject* parent)
    : QObject(parent)
{
    // 排行榜文件位置：AppData/Local/SpectrumFall/leaderboard.json
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    m_path = dataDir + QStringLiteral("/leaderboard.json");

    loadFromDisk();
}

QVector<LeaderboardEntry> LeaderboardManager::allEntries() const
{
    return m_entries;
}

QVector<LeaderboardEntry> LeaderboardManager::entriesForSong(qint64 songFileSize, int laneCount, bool survival) const
{
    QVector<LeaderboardEntry> out;
    for (const LeaderboardEntry& e : m_entries) {
        if (e.songFileSize == songFileSize && e.laneCount == laneCount && e.survival == survival) {
            out.append(e);
        }
    }
    // 分数降序，同分者新的靠前
    std::sort(out.begin(), out.end(), [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.playedAt > b.playedAt;
    });
    return out;
}

QVector<LeaderboardEntry> LeaderboardManager::topEntries(qint64 songFileSize, int laneCount, int limit, bool survival) const
{
    QVector<LeaderboardEntry> list = entriesForSong(songFileSize, laneCount, survival);
    if (list.size() > limit) {
        list = list.mid(0, limit);
    }
    return list;
}

int LeaderboardManager::addEntry(const LeaderboardEntry& entry)
{
    m_entries.append(entry);
    trimAndSort();
    saveToDisk();

    // 计算本次排名
    QVector<LeaderboardEntry> list = entriesForSong(entry.songFileSize, entry.laneCount, entry.survival);
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].playedAt == entry.playedAt && list[i].playerName == entry.playerName) {
            return i + 1;
        }
    }
    return 0; // 未进入前 50 名
}

void LeaderboardManager::removeEntry(const QDateTime& playedAt, const QString& playerName, int score)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].playedAt == playedAt
            && m_entries[i].playerName == playerName
            && m_entries[i].score == score) {
            m_entries.removeAt(i);
            saveToDisk();
            return;
        }
    }
}

void LeaderboardManager::clearSong(qint64 songFileSize, int laneCount, bool survival)
{
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries[i].songFileSize == songFileSize && m_entries[i].laneCount == laneCount
            && m_entries[i].survival == survival) {
            m_entries.removeAt(i);
        }
    }
    saveToDisk();
}

bool LeaderboardManager::exportToFile(const QString& filePath) const
{
    QJsonArray arr;
    for (const LeaderboardEntry& e : m_entries) {
        QJsonObject obj;
        obj[QStringLiteral("songFileName")]   = e.songFileName;
        obj[QStringLiteral("songFileSize")]   = static_cast<double>(e.songFileSize);
        obj[QStringLiteral("songDurationMs")] = static_cast<double>(e.songDurationMs);
        obj[QStringLiteral("bpm")]            = static_cast<double>(e.bpm);
        obj[QStringLiteral("laneCount")]      = e.laneCount;
        obj[QStringLiteral("survival")]       = e.survival;
        obj[QStringLiteral("playerName")]     = e.playerName;
        obj[QStringLiteral("score")]          = e.score;
        obj[QStringLiteral("perfect")]        = e.perfect;
        obj[QStringLiteral("good")]           = e.good;
        obj[QStringLiteral("miss")]           = e.miss;
        obj[QStringLiteral("maxCombo")]       = e.maxCombo;
        obj[QStringLiteral("totalNotes")]     = e.totalNotes;
        obj[QStringLiteral("grade")]          = e.grade;
        obj[QStringLiteral("playedAt")]       = e.playedAt.toString(Qt::ISODate);
        arr.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("myName")]  = m_myName;
    root[QStringLiteral("entries")] = arr;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

int LeaderboardManager::importFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return -1;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return -1;
    QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt(0) != 1) return -1;

    // 建立现有条目的去重集合（playedAt + playerName + score）
    QSet<QString> existingKeys;
    for (const LeaderboardEntry& e : m_entries) {
        QString key = e.playedAt.toString(Qt::ISODate) + "|" + e.playerName + "|" + QString::number(e.score);
        existingKeys.insert(key);
    }

    int imported = 0;
    QJsonArray arr = root.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue& val : arr) {
        QJsonObject obj = val.toObject();
        LeaderboardEntry e;
        e.songFileName   = obj.value(QStringLiteral("songFileName")).toString();
        e.songFileSize   = static_cast<qint64>(obj.value(QStringLiteral("songFileSize")).toDouble(0));
        e.songDurationMs = static_cast<qint64>(obj.value(QStringLiteral("songDurationMs")).toDouble(0));
        e.bpm            = static_cast<float>(obj.value(QStringLiteral("bpm")).toDouble(0));
        e.laneCount      = obj.value(QStringLiteral("laneCount")).toInt(6);
        e.survival       = obj.value(QStringLiteral("survival")).toBool(false);
        e.playerName     = obj.value(QStringLiteral("playerName")).toString();
        e.score          = obj.value(QStringLiteral("score")).toInt(0);
        e.perfect        = obj.value(QStringLiteral("perfect")).toInt(0);
        e.good           = obj.value(QStringLiteral("good")).toInt(0);
        e.miss           = obj.value(QStringLiteral("miss")).toInt(0);
        e.maxCombo       = obj.value(QStringLiteral("maxCombo")).toInt(0);
        e.totalNotes     = obj.value(QStringLiteral("totalNotes")).toInt(0);
        e.grade          = obj.value(QStringLiteral("grade")).toString();
        e.playedAt       = QDateTime::fromString(
            obj.value(QStringLiteral("playedAt")).toString(), Qt::ISODate);

        QString key = e.playedAt.toString(Qt::ISODate) + "|" + e.playerName + "|" + QString::number(e.score);
        if (!existingKeys.contains(key)) {
            m_entries.append(e);
            existingKeys.insert(key);
            ++imported;
        }
    }

    if (imported > 0) {
        trimAndSort();
        saveToDisk();
    }
    return imported;
}

QString LeaderboardManager::myName() const
{
    return m_myName;
}

void LeaderboardManager::setMyName(const QString& name)
{
    if (m_myName == name) return;
    m_myName = name;
    saveToDisk();
}

void LeaderboardManager::loadFromDisk()
{
    m_entries.clear();

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return; // 文件不存在，空榜
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    int version = root.value(QStringLiteral("version")).toInt(0);
    if (version != 1) return; // 版本不匹配，忽略

    m_myName = root.value(QStringLiteral("myName")).toString(QStringLiteral("Player"));

    QJsonArray arr = root.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue& val : arr) {
        QJsonObject obj = val.toObject();

        LeaderboardEntry e;
        e.songFileName   = obj.value(QStringLiteral("songFileName")).toString();
        e.songFileSize   = static_cast<qint64>(obj.value(QStringLiteral("songFileSize")).toDouble(0));
        e.songDurationMs = static_cast<qint64>(obj.value(QStringLiteral("songDurationMs")).toDouble(0));
        e.bpm            = static_cast<float>(obj.value(QStringLiteral("bpm")).toDouble(0));
        e.laneCount      = obj.value(QStringLiteral("laneCount")).toInt(6);
        e.survival       = obj.value(QStringLiteral("survival")).toBool(false);
        e.playerName     = obj.value(QStringLiteral("playerName")).toString(QStringLiteral("Player"));
        e.score          = obj.value(QStringLiteral("score")).toInt(0);
        e.perfect        = obj.value(QStringLiteral("perfect")).toInt(0);
        e.good           = obj.value(QStringLiteral("good")).toInt(0);
        e.miss           = obj.value(QStringLiteral("miss")).toInt(0);
        e.maxCombo       = obj.value(QStringLiteral("maxCombo")).toInt(0);
        e.totalNotes     = obj.value(QStringLiteral("totalNotes")).toInt(0);
        e.grade          = obj.value(QStringLiteral("grade")).toString(QStringLiteral("D"));
        e.playedAt       = QDateTime::fromString(
            obj.value(QStringLiteral("playedAt")).toString(), Qt::ISODate);

        m_entries.append(e);
    }

    trimAndSort();
}

void LeaderboardManager::saveToDisk()
{
    QJsonArray arr;
    for (const LeaderboardEntry& e : m_entries) {
        QJsonObject obj;
        obj[QStringLiteral("songFileName")]   = e.songFileName;
        obj[QStringLiteral("songFileSize")]   = static_cast<double>(e.songFileSize);
        obj[QStringLiteral("songDurationMs")] = static_cast<double>(e.songDurationMs);
        obj[QStringLiteral("bpm")]            = static_cast<double>(e.bpm);
        obj[QStringLiteral("laneCount")]      = e.laneCount;
        obj[QStringLiteral("survival")]       = e.survival;
        obj[QStringLiteral("playerName")]     = e.playerName;
        obj[QStringLiteral("score")]          = e.score;
        obj[QStringLiteral("perfect")]        = e.perfect;
        obj[QStringLiteral("good")]           = e.good;
        obj[QStringLiteral("miss")]           = e.miss;
        obj[QStringLiteral("maxCombo")]       = e.maxCombo;
        obj[QStringLiteral("totalNotes")]     = e.totalNotes;
        obj[QStringLiteral("grade")]          = e.grade;
        obj[QStringLiteral("playedAt")]       = e.playedAt.toString(Qt::ISODate);
        arr.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("myName")]  = m_myName.isEmpty() ? QStringLiteral("Player") : m_myName;
    root[QStringLiteral("entries")] = arr;

    QJsonDocument doc(root);
    QFile file(m_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}

void LeaderboardManager::trimAndSort()
{
    // 先按 (歌曲, 键数, 生存模式, 分数降序, 时间倒序) 排成全序，使同组连续
    std::sort(m_entries.begin(), m_entries.end(), [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
        if (a.songFileSize != b.songFileSize) return a.songFileSize < b.songFileSize;
        if (a.laneCount != b.laneCount) return a.laneCount < b.laneCount;
        if (a.survival != b.survival) return a.survival < b.survival;
        if (a.score != b.score) return a.score > b.score;
        return a.playedAt > b.playedAt;
    });

    // 每组保留前 50
    QVector<LeaderboardEntry> kept;
    kept.reserve(m_entries.size());
    qint64 curFile = 0;
    int curLane = 0;
    bool curSurvival = false;
    int curCount = 0;
    bool first = true;
    for (const LeaderboardEntry& e : m_entries) {
        if (first || e.songFileSize != curFile || e.laneCount != curLane || e.survival != curSurvival) {
            curFile = e.songFileSize;
            curLane = e.laneCount;
            curSurvival = e.survival;
            curCount = 0;
            first = false;
        }
        if (curCount < 50) {
            kept.append(e);
            ++curCount;
        }
    }
    m_entries = std::move(kept);
}
