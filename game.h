#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QFutureWatcher>
#include "NoteGenerator.h"

class AudioEngine;
class BeatDetector;
class NoteGenerator;
class ScoreManager;
class MainMenuWidget;
class SongSelectWidget;
class VisualizationWidget;
class GameWidget;
class ResultWidget;

/// 主窗口：QStackedWidget 容器，管理 5 个页面的导航
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onSongSelectRequested();
    void onSongSelected(const QString& path);
    void onVisualizeRequested();
    void onGameRequested();
    void onGameFinished();
    void onRetryRequested();
    void onBackToMenuRequested();
    void onLoadAndAnalyzeFinished();

private:
    /// 导航到指定页面（0=主菜单, 1=歌曲选择, 2=可视化, 3=游戏, 4=结算）
    void navigateTo(int pageIndex);

    /// 构建 UI
    void setupUI();

    /// 连接所有信号槽
    void connectSignals();

    QStackedWidget* m_stack;             ///< 页面堆栈
    AudioEngine* m_audioEngine;          ///< 音频引擎
    BeatDetector* m_beatDetector;        ///< 节拍检测器
    NoteGenerator* m_noteGenerator;      ///< 音符生成器
    ScoreManager* m_scoreManager;        ///< 分数管理器
    MainMenuWidget* m_mainMenuPage;      ///< 主菜单页
    SongSelectWidget* m_songSelectPage;  ///< 歌曲选择页
    VisualizationWidget* m_visPage;      ///< 可视化页
    GameWidget* m_gamePage;              ///< 游戏页
    ResultWidget* m_resultPage;          ///< 结算页

    QString m_currentSongPath;           ///< 当前选中的歌曲路径
    QVector<GameNote> m_currentNotes;    ///< 当前游戏的音符列表

    // 异步加载相关
    QFutureWatcher<void>* m_loadWatcher; ///< 异步加载监听器
    QString m_pendingSongPath;           ///< 正在加载的歌曲路径
    bool m_loadSuccess;                  ///< 加载是否成功
    float m_analyzedBpm;                 ///< 异步分析出的 BPM
};
