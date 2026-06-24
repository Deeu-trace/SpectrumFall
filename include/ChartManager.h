#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>

/// 谱面条目：用户编辑后的音符数据
struct ChartEntry
{
    QString songFileName;   ///< 文件名（显示用）
    qint64  songFileSize;   ///< 文件大小（字节），作为谱面主键
    qint64  songDurationMs; ///< 音频时长（毫秒）
    float   bpm;            ///< 检测到的 BPM
    int     laneCount;      ///< 轨道数（始终为 6）
    QDateTime editedAt;     ///< 最后编辑时间

    /// 音符数据：每对 [timestampMs, lane]
    QVector<QPair<qint64, int>> notes;
};

/// 谱面管理器：JSON 文件持久化，镜像 CacheManager 范式
class ChartManager : public QObject
{
    Q_OBJECT

public:
    explicit ChartManager(QObject* parent = nullptr);

    /// 是否存在指定歌曲的已保存谱面
    bool hasChart(qint64 songFileSize) const;

    /// 根据 songFileSize 查找谱面，未找到返回 nullptr
    const ChartEntry* findChart(qint64 songFileSize) const;

    /// 获取所有谱面（按 editedAt 倒序）
    QVector<ChartEntry> allCharts() const;

    /// 添加或更新谱面（按 songFileSize 匹配，已存在则覆盖）
    void saveChart(const ChartEntry& entry);

    /// 删除指定歌曲的谱面
    void removeChart(qint64 songFileSize);

private:
    QString m_path;                 ///< 谱面文件路径 (charts.json)
    QVector<ChartEntry> m_entries;  ///< 内存中的谱面数据

    void loadFromDisk();
    void saveToDisk();
};
