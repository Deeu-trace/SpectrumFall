#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QFutureWatcher>
#include <QTimer>
#include "NoteGenerator.h"

class AudioEngine;
class BeatDetector;
class NoteGenerator;
class ScoreManager;
class CacheManager;
class LeaderboardManager;
class ChartManager;
class MainMenuWidget;
class SongSelectWidget;
class VisualizationWidget;
class GameWidget;
class ResultWidget;
class LeaderboardWidget;
class ChartEditorWidget;

/// 主窗口：QStackedWidget 容器，管理 7 个页面的导航
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onSongSelectRequested();
    void onLeaderboardRequested();
    void onAnalyzeRequested(const QString& path);
    void onVisualizeRequested();
    void onGameRequested();
    void onGameFinished();
    void onRetryRequested();
    void onBackToMenuRequested();
    void onScoreSubmitted(const QString& playerName);
    void onAnalyzeFinished();
    void onAnalysisTimeout();

    /// 从缓存加载历史歌曲（跳过分析）
    void onHistorySelected(const QString& filePath);

    /// 删除历史记录
    void onHistoryDeleteRequested(const QString& filePath);

    /// 打开谱面编辑器
    void onChartEditRequested();

    /// 用编辑后的音符开始游戏
    void onChartPlayRequested(const QVector<GameNote>& notes);

private:
    /// 导航到指定页面（0=主菜单, 1=歌曲选择, 2=可视化, 3=游戏, 4=结算, 5=排行榜, 6=谱面编辑）
    void navigateTo(int pageIndex);

    /// 构建 UI
    void setupUI();

    /// 连接所有信号槽
    void connectSignals();

    /// 取消正在进行的分析
    void cancelAnalysis();

    /// 保存当前分析结果到缓存
    void saveToCache();

    /// 刷新歌曲选择页的历史列表
    void refreshHistory();

    QStackedWidget* m_stack;             ///< 页面堆栈
    AudioEngine* m_audioEngine;          ///< 音频引擎
    BeatDetector* m_beatDetector;        ///< 节拍检测器
    NoteGenerator* m_noteGenerator;      ///< 音符生成器
    ScoreManager* m_scoreManager;        ///< 分数管理器
    CacheManager* m_cacheManager;        ///< 缓存管理器
    LeaderboardManager* m_leaderboardManager;  ///< 排行榜管理器
    ChartManager* m_chartManager;        ///< 谱面管理器
    MainMenuWidget* m_mainMenuPage;      ///< 主菜单页
    SongSelectWidget* m_songSelectPage;  ///< 歌曲选择页
    VisualizationWidget* m_visPage;      ///< 可视化页
    GameWidget* m_gamePage;              ///< 游戏页
    ResultWidget* m_resultPage;          ///< 结算页
    LeaderboardWidget* m_leaderboardPage; ///< 排行榜页
    ChartEditorWidget* m_chartEditorPage; ///< 谱面编辑页

    QString m_currentSongPath;           ///< 当前选中的歌曲路径
    QVector<GameNote> m_currentNotes;    ///< 当前歌曲的原始音符（6键，不被4K重映射修改）
    QVector<GameNote> m_gameNotes;       ///< 当前游戏使用的音符（4K时从m_currentNotes拷贝并重映射）
    QVector<GameNote> m_pendingNotes;    ///< 后台线程生成的音符（完成后移入 m_currentNotes）

    // 异步加载相关
    QFutureWatcher<void>* m_loadWatcher; ///< 异步加载监听器
    bool m_loadSuccess;                  ///< 加载是否成功
    float m_analyzedBpm;                 ///< 异步分析出的 BPM
    QString m_errorMessage;              ///< 后台线程写入的错误信息

    // 分析超时
    QTimer* m_analysisTimer;             ///< 90s 超时定时器
    bool m_analysisActive;               ///< 分析是否正在进行
    int m_gameLaneCount;                 ///< 当前游戏键数（4 或 6）

    // ── 跨线程信号（供后台分析 lambda 通过 QueuedConnection 安全更新 UI）──
signals:
    void analysisProgressChanged(int percent);
    void durationUpdated(qint64 durationMs);
};
