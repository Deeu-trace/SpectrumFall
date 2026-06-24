#include "ChartManager.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>

ChartManager::ChartManager(QObject* parent)
    : QObject(parent)
{
    // 谱面文件位置：AppData/Local/SpectrumFall/charts.json
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    m_path = dataDir + QStringLiteral("/charts.json");

    loadFromDisk();
}

bool ChartManager::hasChart(qint64 songFileSize) const
{
    return findChart(songFileSize) != nullptr;
}

const ChartEntry* ChartManager::findChart(qint64 songFileSize) const
{
    for (const ChartEntry& e : m_entries) {
        if (e.songFileSize == songFileSize) {
            return &e;
        }
    }
    return nullptr;
}

QVector<ChartEntry> ChartManager::allCharts() const
{
    return m_entries;
}

void ChartManager::saveChart(const ChartEntry& entry)
{
    // 查找是否已存在
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].songFileSize == entry.songFileSize) {
            m_entries[i] = entry;
            saveToDisk();
            return;
        }
    }
    // 新条目
    m_entries.append(entry);
    saveToDisk();
}

void ChartManager::removeChart(qint64 songFileSize)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].songFileSize == songFileSize) {
            m_entries.removeAt(i);
            saveToDisk();
            return;
        }
    }
}

void ChartManager::loadFromDisk()
{
    m_entries.clear();

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return; // 文件不存在，空谱面
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    int version = root.value(QStringLiteral("version")).toInt(0);
    if (version != 1) return; // 版本不匹配，忽略

    QJsonArray arr = root.value(QStringLiteral("charts")).toArray();
    for (const QJsonValue& val : arr) {
        QJsonObject obj = val.toObject();

        ChartEntry entry;
        entry.songFileName   = obj.value(QStringLiteral("songFileName")).toString();
        entry.songFileSize   = static_cast<qint64>(obj.value(QStringLiteral("songFileSize")).toDouble(0));
        entry.songDurationMs = static_cast<qint64>(obj.value(QStringLiteral("songDurationMs")).toDouble(0));
        entry.bpm            = static_cast<float>(obj.value(QStringLiteral("bpm")).toDouble(0));
        entry.laneCount      = obj.value(QStringLiteral("laneCount")).toInt(6);
        entry.editedAt       = QDateTime::fromString(
            obj.value(QStringLiteral("editedAt")).toString(), Qt::ISODate);

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

    // 按 editedAt 倒序排列（最新在前）
    std::sort(m_entries.begin(), m_entries.end(),
              [](const ChartEntry& a, const ChartEntry& b) {
                  return a.editedAt > b.editedAt;
              });
}

void ChartManager::saveToDisk()
{
    // 按 editedAt 倒序
    std::sort(m_entries.begin(), m_entries.end(),
              [](const ChartEntry& a, const ChartEntry& b) {
                  return a.editedAt > b.editedAt;
              });

    QJsonArray arr;
    for (const ChartEntry& entry : m_entries) {
        QJsonObject obj;
        obj[QStringLiteral("songFileName")]   = entry.songFileName;
        obj[QStringLiteral("songFileSize")]   = static_cast<double>(entry.songFileSize);
        obj[QStringLiteral("songDurationMs")] = static_cast<double>(entry.songDurationMs);
        obj[QStringLiteral("bpm")]            = static_cast<double>(entry.bpm);
        obj[QStringLiteral("laneCount")]      = entry.laneCount;
        obj[QStringLiteral("editedAt")]       = entry.editedAt.toString(Qt::ISODate);

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
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("charts")]  = arr;

    QJsonDocument doc(root);
    QFile file(m_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}
