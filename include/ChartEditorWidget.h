#pragma once

#include <QWidget>
#include <QVector>
#include "NoteGenerator.h"  // for GameNote

class AudioEngine;
class ChartManager;
class ChartTimelineWidget;
class QTableWidget;
class QTableWidgetItem;
class QPushButton;
class QLabel;
class QTimer;
class QKeyEvent;
class QScrollArea;
class QSlider;

/// 谱面编辑器：表格化编辑音符（时间 + 轨道 + 类型 + 持续时间），左键添加/右键删除、空格播放
class ChartEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartEditorWidget(AudioEngine* audio, ChartManager* chartMgr, QWidget* parent = nullptr);

    /// 打开编辑器时调用，载入音符与歌曲元数据
    void loadChart(const QVector<GameNote>& notes, const QString& songPath,
                   qint64 songFileSize, qint64 durationMs, float bpm);

signals:
    /// 用编辑后的音符开始游戏
    void playRequested(const QVector<GameNote>& notes);
    /// 返回歌曲选择页
    void backRequested();

private slots:
    void onPlayPause();
    void onSaveChart();
    void onPlayGame();
    void onItemChanged(QTableWidgetItem* item);
    void onPositionUpdate();
    /// 时间轴左键点击空白处：添加音符
    void onTimelineNoteAdded(qint64 timeMs, int lane);
    /// 时间轴左键点击已有音符：选中对应表格行
    void onTimelineNoteSelected(int noteIndex);
    /// 时间轴右键点击音符：删除
    void onTimelineNoteDeleted(int noteIndex);
    /// 时间轴拖拽创建 HOLD 音符
    void onTimelineHoldAdded(qint64 startTimeMs, qint64 durationMs, int lane);
    /// 切换 4K/6K 模式
    void onKeyModeToggled();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    AudioEngine* m_audio;
    ChartManager* m_chartMgr;

    // 歌曲元数据
    QString m_songFileName;
    QString m_songPath;
    qint64  m_songFileSize;
    qint64  m_songDurationMs;
    float   m_bpm;
    int     m_laneCount;       ///< 当前编辑模式：6 或 4

    // UI
    QLabel*       m_infoLabel;      ///< 歌曲名 | BPM | 音符数 | 时长
    ChartTimelineWidget* m_timeline; ///< 可视化时间轴
    QScrollArea*  m_timelineScroll;  ///< 时间轴滚动容器
    QSlider*      m_zoomSlider;      ///< 缩放滑块
    QLabel*       m_zoomLabel;       ///< 缩放百分比标签
    QTableWidget* m_table;          ///< 4 列：时间(ms), 轨道, 类型, 持续时间
    QLabel*       m_posLabel;       ///< 当前播放位置
    QPushButton*  m_keyModeBtn;     ///< 4K/6K 切换按钮
    QPushButton*  m_saveBtn;       ///< 保存谱面
    QPushButton*  m_playGameBtn;   ///< 开始游戏
    QPushButton*  m_backBtn;       ///< 返回
    QTimer*       m_posTimer;      ///< 100ms 轮询播放位置
    QTimer*       m_syncTimer;     ///< 批量同步定时器（性能优化）

    bool m_loading;  ///< 抑制 itemChanged 信号（程序化填充/排序时）
    bool m_syncPending;  ///< 是否有待处理的 syncTimeline

    void setupUI();
    void populateTable(const QVector<GameNote>& notes);
    QVector<GameNote> collectNotes() const;
    void sortTable();
    void syncTimeline();  ///< 将表格当前音符同步到时间轴
    void scheduleSync();  ///< 延迟批量同步（100ms 内合并多次请求）
    void updateInfoLabel();
    QString formatTime(qint64 ms) const;
    void applyZoom(int sliderValue); ///< 处理缩放滑块变化
};
