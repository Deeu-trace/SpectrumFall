#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include "NoteGenerator.h"  // for GameNote

class AudioEngine;
class QTimer;

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

    /// 闪烁指定轨道（键盘实时录入时的视觉反馈）
    void flashLane(int lane);

    /// 设置缩放因子 (1.0 ~ 10.0)，影响时间轴水平宽度
    void setZoomFactor(float factor);
    float zoomFactor() const { return m_zoomFactor; }

    /// 应用当前缩放到实际几何（由 debounce 定时器调用）
    void applyZoomGeometry();

    /// 是否正在拖拽播放头（供外部跳过位置更新）
    bool isDraggingPlayhead() const { return m_draggingPlayhead; }

    /// 设置选中的音符索引（绘制白边高亮），-1 = 无选中
    void setSelectedIndex(int index);

    /// 设置当前轨道数（4 或 6），影响轨道显示和命中检测
    void setLaneCount(int count);
    int laneCount() const { return m_laneCount; }

    QSize minimumSizeHint() const override { return QSize(200, 200); }
    QSize sizeHint() const override;

    /// 时间转像素坐标（供外部自动滚动使用）
    int timeToX(qint64 ms) const;

signals:
    /// 用户点击空白处，请求添加音符
    void noteAdded(qint64 timeMs, int lane);

    /// 用户点击已有音符，请求选中（index 对应 setNotes 传入的 vector 索引）
    void noteSelected(int index);

    /// 用户右键点击音符，请求删除
    void noteDeleted(int index);

    /// 用户拖拽创建 HOLD 音符（起始时间, 持续时间, 轨道）
    void holdNoteAdded(qint64 startTimeMs, qint64 durationMs, int lane);

    /// 缩放因子变化（由 Ctrl+滚轮触发）
    void zoomChanged(float factor);

    /// 播放头被拖拽到新位置
    void playheadDragged(qint64 timeMs);

    /// 请求编辑器水平滚动时间轴（delta 像素，正=向右/更晚，负=向左/更早）
    void scrollRequested(int deltaPixels);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    AudioEngine* m_audio;
    QVector<GameNote> m_notes;
    int m_selectedIndex;   ///< 选中的音符索引（白边高亮），-1 = 无
    qint64 m_durationMs;
    qint64 m_positionMs;

    // 波形缓存
    struct WaveformBin { float peak; };
    QVector<WaveformBin> m_waveform;  ///< 降采样后的峰值数组
    bool m_waveformValid;             ///< 波形是否已构建

    // 布局常量
    static constexpr int WAVEFORM_HEIGHT = 60;   ///< 波形区高度（像素）
    static constexpr int LANE_COUNT = 6;          ///< 最大轨道数（数组大小）
    static constexpr int NOTE_HIT_RADIUS = 7;     ///< 点击命中半径（像素）
    static constexpr int BASE_WIDTH = 2400;       ///< 1x 缩放时的基础宽度（约显示 60s/3min 歌曲）

    // 轨道颜色
    static const QColor LANE_COLORS[LANE_COUNT];

    int m_laneCount = LANE_COUNT;  ///< 当前活动轨道数（4 或 6）

    // 坐标转换
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

    /// 判断 x 坐标是否在播放头附近（用于拖拽命中检测）
    bool isNearPlayhead(int x) const;

    // ── 轨道闪烁（实时录入反馈）──
    int    m_flashLane;      ///< 当前闪烁轨道 (-1 = 无)
    int    m_flashAlpha;     ///< 当前闪烁透明度
    QTimer* m_flashTimer;    ///< 驱动闪烁衰减

    // ── 缩放 ──
    float   m_zoomFactor;     ///< 水平缩放因子 (1.0 ~ 10.0)
    QTimer* m_zoomDebounce;   ///< 50ms 防抖定时器，避免拖拽滑块时频繁重建波形

    // ── 播放头拖拽 ──
    bool m_draggingPlayhead;  ///< 是否正在拖拽播放头
    bool m_wasPlayingBeforeDrag; ///< 拖拽前音频是否在播放

    // ── 拖拽创建 HOLD ──
    bool    m_draggingToCreate;     ///< 是否正在拖拽创建 HOLD 音符
    int     m_dragStartX;           ///< 拖拽起始 x 坐标
    qint64  m_dragStartTimeMs;      ///< 拖拽起始时间
    int     m_dragLane;             ///< 拖拽所在轨道
    int     m_dragCurrentX;         ///< 拖拽当前 x（用于预览绘制）

    static constexpr int PLAYHEAD_GRAB_RADIUS = 10; ///< 播放头拖拽命中半径（像素）
    static constexpr int MIN_DRAG_THRESHOLD = 8;    ///< 拖拽创建 HOLD 的最小像素距离
};
