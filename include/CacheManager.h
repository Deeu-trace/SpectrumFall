#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>

/// 缓存条目：一次分析的结果
struct CacheEntry
{
    QString filePath;      ///< 音频文件绝对路径（缓存键）
    QString fileName;      ///< 文件名（显示用）
    qint64  fileSize;      ///< 文件大小（字节），用于检测文件是否被替换
    qint64  durationMs;    ///< 音频时长（毫秒）
    float   bpm;           ///< 检测到的 BPM
    QDateTime analyzedAt;  ///< 最后分析时间

    /// 音符数据：每对 [timestampMs, lane]
    QVector<QPair<qint64, int>> notes;
};

/// 分析结果缓存管理器：JSON 文件持久化
class CacheManager : public QObject
{
    Q_OBJECT

public:
    explicit CacheManager(QObject* parent = nullptr);

    /// 获取所有缓存条目（按 analyzedAt 倒序）
    QVector<CacheEntry> allEntries() const;

    /// 根据 filePath 查找缓存条目，未找到返回 nullptr
    const CacheEntry* find(const QString& filePath) const;

    /// 添加或更新缓存条目（已存在则覆盖）
    void upsert(const CacheEntry& entry);

    /// 删除指定路径的缓存条目
    void remove(const QString& filePath);

    /// 检查指定文件是否有有效缓存（文件仍存在且大小未变）
    bool hasValidCache(const QString& filePath) const;

private:
    /// 缓存文件路径
    QString m_cachePath;

    /// 内存中的缓存数据
    QVector<CacheEntry> m_entries;

    /// 从磁盘加载缓存
    void loadFromDisk();

    /// 保存缓存到磁盘
    void saveToDisk();
};
