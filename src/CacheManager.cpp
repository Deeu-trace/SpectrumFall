#include "CacheManager.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>

CacheManager::CacheManager(QObject* parent)
    : QObject(parent)
{
    // 缓存文件位置：AppData/Local/SpectrumFall/cache.json
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    m_cachePath = dataDir + QStringLiteral("/cache.json");

    loadFromDisk();
}

QVector<CacheEntry> CacheManager::allEntries() const
{
    return m_entries;
}

const CacheEntry* CacheManager::find(const QString& filePath) const
{
    for (const CacheEntry& e : m_entries) {
        if (e.filePath == filePath) {
            return &e;
        }
    }
    return nullptr;
}

void CacheManager::upsert(const CacheEntry& entry)
{
    // 查找是否已存在
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].filePath == entry.filePath) {
            m_entries[i] = entry;
            saveToDisk();
            return;
        }
    }
    // 新条目
    m_entries.append(entry);
    saveToDisk();
}

void CacheManager::remove(const QString& filePath)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].filePath == filePath) {
            m_entries.removeAt(i);
            saveToDisk();
            return;
        }
    }
}

bool CacheManager::hasValidCache(const QString& filePath) const
{
    const CacheEntry* entry = find(filePath);
    if (!entry) return false;

    // 文件必须仍然存在
    QFileInfo fi(filePath);
    if (!fi.exists()) return false;

    // 文件大小未变（简单检测文件是否被替换）
    if (fi.size() != entry->fileSize) return false;

    return true;
}

void CacheManager::loadFromDisk()
{
    m_entries.clear();

    QFile file(m_cachePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return; // 文件不存在，空缓存
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    int version = root.value(QStringLiteral("version")).toInt(0);
    if (version != 1) return; // 版本不匹配，忽略

    QJsonArray arr = root.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue& val : arr) {
        QJsonObject obj = val.toObject();

        CacheEntry entry;
        entry.filePath   = obj.value(QStringLiteral("filePath")).toString();
        entry.fileName   = obj.value(QStringLiteral("fileName")).toString();
        entry.fileSize   = static_cast<qint64>(obj.value(QStringLiteral("fileSize")).toDouble(0));
        entry.durationMs = static_cast<qint64>(obj.value(QStringLiteral("durationMs")).toDouble(0));
        entry.bpm        = static_cast<float>(obj.value(QStringLiteral("bpm")).toDouble(0));
        entry.lowFreqRatio = static_cast<float>(obj.value(QStringLiteral("lowFreqRatio")).toDouble(0));
        entry.avgEnergy   = static_cast<float>(obj.value(QStringLiteral("avgEnergy")).toDouble(0));
        entry.analyzedAt = QDateTime::fromString(
            obj.value(QStringLiteral("analyzedAt")).toString(), Qt::ISODate);

        // 解析音符数组
        QJsonArray notesArr = obj.value(QStringLiteral("notes")).toArray();
        entry.notes.reserve(notesArr.size());
        for (const QJsonValue& nv : notesArr) {
            QJsonArray pair = nv.toArray();
            if (pair.size() == 2) {
                entry.notes.append({static_cast<qint64>(pair[0].toDouble()),
                                    pair[1].toInt()});
            }
        }

        m_entries.append(entry);
    }

    // 按 analyzedAt 倒序排列（最新在前）
    std::sort(m_entries.begin(), m_entries.end(),
              [](const CacheEntry& a, const CacheEntry& b) {
                  return a.analyzedAt > b.analyzedAt;
              });
}

void CacheManager::saveToDisk()
{
    // 按 analyzedAt 倒序
    std::sort(m_entries.begin(), m_entries.end(),
              [](const CacheEntry& a, const CacheEntry& b) {
                  return a.analyzedAt > b.analyzedAt;
              });

    QJsonArray arr;
    for (const CacheEntry& entry : m_entries) {
        QJsonObject obj;
        obj[QStringLiteral("filePath")]   = entry.filePath;
        obj[QStringLiteral("fileName")]   = entry.fileName;
        obj[QStringLiteral("fileSize")]   = static_cast<double>(entry.fileSize);
        obj[QStringLiteral("durationMs")] = static_cast<double>(entry.durationMs);
        obj[QStringLiteral("bpm")]        = static_cast<double>(entry.bpm);
        obj[QStringLiteral("lowFreqRatio")] = static_cast<double>(entry.lowFreqRatio);
        obj[QStringLiteral("avgEnergy")]   = static_cast<double>(entry.avgEnergy);
        obj[QStringLiteral("analyzedAt")] = entry.analyzedAt.toString(Qt::ISODate);

        QJsonArray notesArr;
        for (const auto& note : entry.notes) {
            QJsonArray pair;
            pair.append(static_cast<double>(note.first));
            pair.append(note.second);
            notesArr.append(pair);
        }
        obj[QStringLiteral("notes")] = notesArr;

        arr.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("version")]  = 1;
    root[QStringLiteral("entries")]  = arr;

    QJsonDocument doc(root);
    QFile file(m_cachePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}
