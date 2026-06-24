#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include "NoteGenerator.h"  // for GameNote

class AudioEngine;

/// 可视化时间轴：波形预览 + 6 轨道色带 + 音符标记 + 播放头
/// 嵌入谱面编辑器表格上方，支持点击添加/选中音符
class ChartTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartTimelineWidget(QWidget* parent = nullptr);

    /// 设置音频引擎（用于提取 PCM 波形）
    void setAudioEngine(AudioEngine* audio);

    /// 设置音符列表（绘制标记用）
    void setNotes(const QVector<GameNote>& notes);

    /// 设置总时长（毫秒）
    void setDurationMs(qint64 ms);

    /// 设置当前播放位置（毫秒），驱动播放头
    void setPositionMs(qint64 ms);

    QSize minimumSizeHint() const override { return QSize(200, 200); }
    QSize sizeHint() const override { return QSize(400, 200); }

signals:
    /// 用户点击空白处，请求添加音符
    void noteAdded(qint64 timeMs, int lane);

    /// 用户点击已有音符，请求选中（index 对应 setNotes 传入的 vector 索引）
    void noteSelected(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    AudioEngine* m_audio;
    QVector<GameNote> m_notes;
    qint64 m_durationMs;
    qint64 m_positionMs;

    // 波形缓存
    struct WaveformBin { float peak; };
    QVector<WaveformBin> m_waveform;  ///< 降采样后的峰值数组
    bool m_waveformValid;             ///< 波形是否已构建

    // 布局常量
    static constexpr int WAVEFORM_HEIGHT = 60;   ///< 波形区高度（像素）
    static constexpr int LANE_COUNT = 6;
    static constexpr int NOTE_HIT_RADIUS = 7;     ///< 点击命中半径（像素）

    // 轨道颜色（红橙黄绿蓝紫）
    static const QColor LANE_COLORS[LANE_COUNT];

    // 坐标转换
    int timeToX(qint64 ms) const;
    qint64 xToTime(int x) const;
    int laneToY(int lane) const;
    int yToLane(int y) const;

    // 布局计算
    int laneAreaTop() const;
    int laneAreaBottom() const;
    int laneHeight() const;

    /// 重建降采样波形（从 AudioEngine PCM 提取）
    void rebuildWaveform();

    /// 查找点击位置附近的音符索引，未命中返回 -1
    int findNoteAt(int x, int y) const;
};
