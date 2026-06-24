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

/// 谱面编辑器：表格化编辑音符（时间戳 + 轨道），支持增删改、音频预览、保存、开始游戏
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
    void onAddNote();
    void onDeleteNotes();
    void onPlayPause();
    void onSeekToNote();
    void onSaveChart();
    void onPlayGame();
    void onItemChanged(QTableWidgetItem* item);
    void onPositionUpdate();
    /// 时间轴点击空白处：添加音符
    void onTimelineNoteAdded(qint64 timeMs, int lane);
    /// 时间轴点击已有音符：选中对应表格行
    void onTimelineNoteSelected(int noteIndex);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    AudioEngine* m_audio;
    ChartManager* m_chartMgr;

    // 歌曲元数据
    QString m_songFileName;
    QString m_songPath;
    qint64  m_songFileSize;
    qint64  m_songDurationMs;
    float   m_bpm;

    // UI
    QLabel*       m_infoLabel;      ///< 歌曲名 | BPM | 音符数 | 时长
    ChartTimelineWidget* m_timeline; ///< 可视化时间轴
    QTableWidget* m_table;          ///< 2 列：时间(ms), 轨道
    QLabel*       m_posLabel;       ///< 当前播放位置
    QPushButton*  m_addBtn;        ///< 添加音符
    QPushButton*  m_delBtn;        ///< 删除选中行
    QPushButton*  m_playBtn;       ///< 播放/暂停
    QPushButton*  m_seekBtn;       ///< 跳到选中音符的时间
    QPushButton*  m_saveBtn;       ///< 保存谱面
    QPushButton*  m_playGameBtn;   ///< 开始游戏
    QPushButton*  m_backBtn;       ///< 返回
    QTimer*       m_posTimer;      ///< 50ms 轮询播放位置

    bool m_loading;  ///< 抑制 itemChanged 信号（程序化填充/排序时）

    void setupUI();
    void populateTable(const QVector<GameNote>& notes);
    QVector<GameNote> collectNotes() const;
    void sortTable();
    void syncTimeline();  ///< 将表格当前音符同步到时间轴
    void updateInfoLabel();
    QString formatTime(qint64 ms) const;
};
