#include "game.h"
#include "AudioEngine.h"
#include "BeatDetector.h"
#include "NoteGenerator.h"
#include "ScoreManager.h"
#include "CacheManager.h"
#include "MainMenuWidget.h"
#include "SongSelectWidget.h"
#include "VisualizationWidget.h"
#include "GameWidget.h"
#include "ResultWidget.h"
#include "LeaderboardWidget.h"
#include "LeaderboardManager.h"
#include "ChartEditorWidget.h"
#include "ChartManager.h"

#include <QApplication>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QEventLoop>
#include <QtConcurrent>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_stack(new QStackedWidget(this))
    , m_audioEngine(new AudioEngine(this))
    , m_beatDetector(new BeatDetector(this))
    , m_noteGenerator(new NoteGenerator())
    , m_scoreManager(new ScoreManager(this))
    , m_cacheManager(new CacheManager(this))
    , m_leaderboardManager(new LeaderboardManager(this))
    , m_chartManager(new ChartManager(this))
    , m_mainMenuPage(nullptr)
    , m_songSelectPage(nullptr)
    , m_visPage(nullptr)
    , m_gamePage(nullptr)
    , m_resultPage(nullptr)
    , m_leaderboardPage(nullptr)
    , m_chartEditorPage(nullptr)
    , m_loadWatcher(new QFutureWatcher<void>(this))
    , m_loadSuccess(false)
    , m_analyzedBpm(0.0f)
    , m_analysisTimer(new QTimer(this))
    , m_analysisActive(false)
    , m_gameLaneCount(6)
{
    setupUI();
    connectSignals();
    navigateTo(0); // 启动时显示主菜单

    // 超时定时器：单次触发 90 秒
    m_analysisTimer->setSingleShot(true);

    // 初始化历史列表
    refreshHistory();
}

MainWindow::~MainWindow()
{
    m_beatDetector->requestCancel();
    m_analysisTimer->stop();
    delete m_noteGenerator;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_beatDetector->requestCancel();
    m_analysisTimer->stop();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("SpectrumFall"));
    setMinimumSize(800, 600);
    resize(1000, 700);

    // 创建 7 个页面
    m_mainMenuPage = new MainMenuWidget(this);
    m_songSelectPage = new SongSelectWidget(this);
    m_visPage = new VisualizationWidget(m_audioEngine, this);
    m_gamePage = new GameWidget(m_audioEngine, m_scoreManager, this);
    m_resultPage = new ResultWidget(this);
    m_leaderboardPage = new LeaderboardWidget(m_leaderboardManager, this);
    m_chartEditorPage = new ChartEditorWidget(m_audioEngine, m_chartManager, this);

    // 添加到 QStackedWidget（按顺序：0-6）
    m_stack->addWidget(m_mainMenuPage);      // 索引 0：主菜单
    m_stack->addWidget(m_songSelectPage);    // 索引 1：歌曲选择
    m_stack->addWidget(m_visPage);           // 索引 2：可视化
    m_stack->addWidget(m_gamePage);          // 索引 3：游戏
    m_stack->addWidget(m_resultPage);        // 索引 4：结算
    m_stack->addWidget(m_leaderboardPage);   // 索引 5：排行榜
    m_stack->addWidget(m_chartEditorPage);   // 索引 6：谱面编辑

    setCentralWidget(m_stack);
}

void MainWindow::connectSignals()
{
    // 主菜单 → 歌曲选择 / 排行榜 / 退出
    connect(m_mainMenuPage, &MainMenuWidget::songSelectRequested,
            this, &MainWindow::onSongSelectRequested);
    connect(m_mainMenuPage, &MainMenuWidget::leaderboardRequested,
            this, &MainWindow::onLeaderboardRequested);
    connect(m_mainMenuPage, &MainMenuWidget::exitRequested,
            QApplication::instance(), &QApplication::quit);

    // 排行榜页 → 返回主菜单
    connect(m_leaderboardPage, &LeaderboardWidget::backRequested,
            this, [this]() { navigateTo(0); });

    // 歌曲选择 → 手动分析
    connect(m_songSelectPage, &SongSelectWidget::analyzeRequested,
            this, &MainWindow::onAnalyzeRequested);
    connect(m_songSelectPage, &SongSelectWidget::visualizeRequested,
            this, &MainWindow::onVisualizeRequested);
    connect(m_songSelectPage, &SongSelectWidget::gameRequested,
            this, &MainWindow::onGameRequested);
    connect(m_songSelectPage, &SongSelectWidget::backRequested,
            this, [this]() {
                cancelAnalysis();
                m_songSelectPage->resetState();
                navigateTo(0);
            });

    // 历史记录交互
    connect(m_songSelectPage, &SongSelectWidget::historySelected,
            this, &MainWindow::onHistorySelected);
    connect(m_songSelectPage, &SongSelectWidget::historyDeleteRequested,
            this, &MainWindow::onHistoryDeleteRequested);

    // 谱面编辑器 → 开始游戏 / 返回
    connect(m_songSelectPage, &SongSelectWidget::chartEditRequested,
            this, &MainWindow::onChartEditRequested);
    connect(m_chartEditorPage, &ChartEditorWidget::playRequested,
            this, &MainWindow::onChartPlayRequested);
    connect(m_chartEditorPage, &ChartEditorWidget::backRequested,
            this, [this]() { m_audioEngine->pause(); navigateTo(1); });

    // 可视化页 → 返回
    connect(m_visPage, &VisualizationWidget::backRequested,
            this, [this]() {
                m_visPage->stopVisualization();
                m_audioEngine->pause();
                navigateTo(1);
            });

    // 游戏页 → 结算
    connect(m_gamePage, &GameWidget::gameFinished,
            this, &MainWindow::onGameFinished);
    connect(m_gamePage, &GameWidget::backRequested,
            this, [this]() { navigateTo(0); });

    // 结算页 → 重试/返回/记入排行榜
    connect(m_resultPage, &ResultWidget::retryRequested,
            this, &MainWindow::onRetryRequested);
    connect(m_resultPage, &ResultWidget::backRequested,
            this, &MainWindow::onBackToMenuRequested);
    connect(m_resultPage, &ResultWidget::scoreSubmitted,
            this, &MainWindow::onScoreSubmitted);

    // 异步分析完成
    connect(m_loadWatcher, &QFutureWatcher<void>::finished,
            this, &MainWindow::onAnalyzeFinished);

    // 超时
    connect(m_analysisTimer, &QTimer::timeout,
            this, &MainWindow::onAnalysisTimeout);

    // 跨线程 UI 更新信号
    connect(this, &MainWindow::analysisProgressChanged,
            m_songSelectPage, &SongSelectWidget::setAnalysisProgress);
    connect(this, &MainWindow::durationUpdated,
            m_songSelectPage, &SongSelectWidget::setDurationDisplay);

    // BeatDetector 进度信号（0-100 映射到 30-85）
    connect(m_beatDetector, &BeatDetector::progressChanged,
            this, [this](int percent) {
                int overall = 30 + percent * 55 / 100;
                emit analysisProgressChanged(overall);
            });
}

void MainWindow::navigateTo(int pageIndex)
{
    m_stack->setCurrentIndex(pageIndex);
}

void MainWindow::onSongSelectRequested()
{
    // 每次进入歌曲选择页时刷新历史
    refreshHistory();
    navigateTo(1);
}

void MainWindow::onLeaderboardRequested()
{
    // 进入排行榜页前刷新，载入最新记录
    m_leaderboardPage->refresh();
    navigateTo(5);
}

void MainWindow::onAnalyzeRequested(const QString& path)
{
    if (m_analysisActive) return;

    // 分析阶段始终用 6 键生成谱面（更细频带），4/6 键选择在"开始游戏"时弹出
    m_gameLaneCount = 6;

    m_currentSongPath = path;
    m_loadSuccess = false;
    m_analyzedBpm = 0.0f;
    m_errorMessage.clear();
    m_pendingNotes.clear();
    m_analysisActive = true;

    // 启动 90 秒超时定时器
    m_analysisTimer->start(300000);

    AudioEngine* engine = m_audioEngine;
    BeatDetector* detector = m_beatDetector;
    NoteGenerator* generator = m_noteGenerator;

    auto future = QtConcurrent::run([this, engine, detector, generator, path]() {
        // ── Phase 1: 加载音频文件（0% → 30%）──
        emit analysisProgressChanged(5);

        m_loadSuccess = engine->loadFile(path);

        if (!m_loadSuccess) {
            m_errorMessage = QStringLiteral("无法加载音频文件，请检查文件是否损坏或格式不受支持");
            return;
        }

        if (detector->isCancelled()) return;
        emit durationUpdated(engine->duration());
        emit analysisProgressChanged(30);

        if (engine->duration() < 10000) {
            m_loadSuccess = false;
            m_errorMessage = QStringLiteral("音频太短（不足 10 秒），无法生成谱面");
            return;
        }

        if (detector->isCancelled()) return;

        // ── Phase 2: 节拍检测（30% → 85%）──
        const QVector<float>& pcm = engine->pcmData();
        detector->analyze(pcm, engine->sampleRate(), m_gameLaneCount);

        if (detector->isCancelled()) return;
        emit analysisProgressChanged(90);

        // ── Phase 3: 生成谱面（90% → 100%）──
        m_pendingNotes = generator->generate(detector->beatPoints(), 200, m_gameLaneCount);
        m_analyzedBpm = detector->bpm();
        m_loadSuccess = true;

        if (detector->beatPoints().isEmpty()) {
            m_errorMessage = QStringLiteral("未检测到明显节拍，请尝试其他歌曲");
        }

        emit analysisProgressChanged(100);
    });

    m_loadWatcher->setFuture(future);
}

void MainWindow::onAnalyzeFinished()
{
    m_analysisTimer->stop();
    m_analysisActive = false;

    if (m_beatDetector->isCancelled()) {
        // 被取消（用户返回或超时后取消），不更新 UI
        return;
    }

    if (m_loadSuccess) {
        // 成功：将后台生成的音符移到主线程
        m_currentNotes = std::move(m_pendingNotes);
        m_songSelectPage->onAnalysisComplete(m_analyzedBpm);

        // 保存到缓存
        saveToCache();
        refreshHistory();

        // 如果没有节拍，也视为成功加载但给出警告
        if (!m_errorMessage.isEmpty()) {
            m_songSelectPage->showAnalysisError(m_errorMessage);
        }
    } else {
        m_songSelectPage->showAnalysisError(
            m_errorMessage.isEmpty()
                ? QStringLiteral("分析失败，请重试或选择其他歌曲")
                : m_errorMessage);
    }
}

void MainWindow::onAnalysisTimeout()
{
    m_beatDetector->requestCancel();
    m_analysisActive = false;
    m_songSelectPage->showAnalysisError(
        QStringLiteral("分析超时（90 秒），请尝试其他歌曲或更短的音频"));
}

void MainWindow::cancelAnalysis()
{
    if (m_analysisActive) {
        m_beatDetector->requestCancel();
        m_analysisTimer->stop();
        // 不等后台线程结束，onAnalyzeFinished 会检测 cancelFlag 并跳过 UI 更新
    }
}

void MainWindow::onVisualizeRequested()
{
    if (m_currentSongPath.isEmpty() || m_analysisActive) return;

    navigateTo(2);
    m_audioEngine->seek(0);
    m_audioEngine->play();
    m_visPage->startVisualization();
}

void MainWindow::onGameRequested()
{
    if (m_currentSongPath.isEmpty() || m_analysisActive) return;
    if (m_currentNotes.isEmpty()) {
        m_songSelectPage->showAnalysisError(
            QStringLiteral("没有可用的谱面音符，请尝试其他歌曲"));
        return;
    }

    // ── 模式选择界面（内嵌 widget，非模态对话框）──
    // 创建一个无边框半透明遮罩 widget，覆盖在歌曲选择页上
    QWidget* overlay = new QWidget(m_songSelectPage);
    overlay->setObjectName("modeSelectOverlay");
    overlay->setGeometry(m_songSelectPage->rect());
    overlay->setStyleSheet("QWidget#modeSelectOverlay { background-color: rgba(5, 5, 20, 230); }");

    QVBoxLayout* ol = new QVBoxLayout(overlay);
    ol->setAlignment(Qt::AlignCenter);
    ol->setSpacing(20);

    QLabel* title = new QLabel(QStringLiteral("选择游戏模式"), overlay);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #00ff88; background: transparent;");
    ol->addWidget(title);

    QHBoxLayout* bl = new QHBoxLayout();
    bl->setSpacing(20);

    QPushButton* btn4 = new QPushButton(QStringLiteral("4 键\n\nD  F  J  K"), overlay);
    btn4->setMinimumSize(180, 120);
    btn4->setStyleSheet(
        "QPushButton { font-size: 20px; font-weight: bold; color: #e0e0e0; "
        "  background-color: #16213e; border: 2px solid #0f3460; border-radius: 12px; }"
        "QPushButton:hover { background-color: #1a1a40; border-color: #00ff88; color: #00ff88; }"
    );
    bl->addWidget(btn4);

    QPushButton* btn6 = new QPushButton(QStringLiteral("6 键\n\nS  D  F  J  K  L"), overlay);
    btn6->setMinimumSize(180, 120);
    btn6->setStyleSheet(
        "QPushButton { font-size: 20px; font-weight: bold; color: #e0e0e0; "
        "  background-color: #16213e; border: 2px solid #0f3460; border-radius: 12px; }"
        "QPushButton:hover { background-color: #1a1a40; border-color: #00ff88; color: #00ff88; }"
    );
    bl->addWidget(btn6);
    ol->addLayout(bl);

    QPushButton* cancelBtn = new QPushButton(QStringLiteral("取消"), overlay);
    cancelBtn->setMinimumSize(100, 36);
    cancelBtn->setStyleSheet(
        "QPushButton { font-size: 14px; color: #8888aa; "
        "  background-color: transparent; border: 1px solid #333; border-radius: 6px; }"
        "QPushButton:hover { color: #ff5577; border-color: #ff5577; }"
    );
    ol->addWidget(cancelBtn);

    overlay->show();
    overlay->raise();

    int mode = 0;
    auto chooseMode = [overlay, &mode](int m) {
        mode = m;
        overlay->deleteLater();
    };
    connect(btn4, &QPushButton::clicked, [chooseMode]() { chooseMode(4); });
    connect(btn6, &QPushButton::clicked, [chooseMode]() { chooseMode(6); });
    connect(cancelBtn, &QPushButton::clicked, overlay, &QWidget::deleteLater);

    // 嵌套事件循环，等 overlay 被 deleteLater 后退出
    QEventLoop loop;
    connect(overlay, &QWidget::destroyed, &loop, &QEventLoop::quit);
    loop.exec();

    if (mode == 0) return;
    m_gameLaneCount = mode;

    // 拷贝一份用于游戏（4K 重映射改副本，不破坏原始 6 键数据）
    m_gameNotes = m_currentNotes;
    if (m_gameLaneCount == 4) {
        static const int laneMap6to4[6] = {0, 0, 1, 2, 3, 3};
        for (auto& note : m_gameNotes) {
            if (note.lane >= 0 && note.lane < 6) {
                note.lane = laneMap6to4[note.lane];
            }
        }
    }

    navigateTo(3);
    m_gamePage->startGame(m_gameNotes, m_gameLaneCount);
}

void MainWindow::onGameFinished()
{
    int totalNotes = m_gameNotes.size();
    m_resultPage->setResult(
        m_scoreManager->score(),
        m_scoreManager->perfectCount(),
        m_scoreManager->goodCount(),
        m_scoreManager->missCount(),
        m_scoreManager->maxCombo(),
        totalNotes
    );
    // 预填上次使用的玩家名，供玩家确认后记入排行榜
    m_resultPage->presetName(m_leaderboardManager->myName());
    navigateTo(4);
}

void MainWindow::onRetryRequested()
{
    m_scoreManager->reset();
    if (m_gameNotes.isEmpty()) {
        navigateTo(1);
        return;
    }
    navigateTo(3);
    m_gamePage->startGame(m_gameNotes, m_gameLaneCount);
}

void MainWindow::onBackToMenuRequested()
{
    m_audioEngine->pause();
    navigateTo(0);
}

// ── 排行榜相关 ───────────────────────────────────────────────
void MainWindow::onScoreSubmitted(const QString& playerName)
{
    // 记住本机玩家名，下次结算预填
    m_leaderboardManager->setMyName(playerName);

    if (m_currentSongPath.isEmpty()) {
        m_resultPage->showRank(0, 0);
        return;
    }

    QFileInfo fi(m_currentSongPath);

    LeaderboardEntry entry;
    entry.songFileName   = fi.fileName();
    entry.songFileSize   = fi.size();
    entry.songDurationMs = m_audioEngine->duration();
    entry.bpm            = m_analyzedBpm;
    entry.laneCount      = m_gameLaneCount;
    entry.playerName     = playerName;
    entry.score          = m_scoreManager->score();
    entry.perfect        = m_scoreManager->perfectCount();
    entry.good           = m_scoreManager->goodCount();
    entry.miss           = m_scoreManager->missCount();
    entry.maxCombo       = m_scoreManager->maxCombo();
    entry.totalNotes     = m_gameNotes.size();
    entry.grade          = m_scoreManager->grade(entry.totalNotes);
    entry.playedAt       = QDateTime::currentDateTime();

    int rank = m_leaderboardManager->addEntry(entry);
    int total = m_leaderboardManager->entriesForSong(entry.songFileSize, entry.laneCount).size();
    m_resultPage->showRank(rank, total);
}

// ── 缓存相关 ────────────────────────────────────────────────

void MainWindow::onHistorySelected(const QString& filePath)
{
    if (m_analysisActive) return;

    // 查找缓存
    const CacheEntry* entry = m_cacheManager->find(filePath);
    if (!entry) {
        m_songSelectPage->showAnalysisError(
            QStringLiteral("缓存记录不存在，请重新分析"));
        return;
    }

    // 验证缓存有效性
    if (!m_cacheManager->hasValidCache(filePath)) {
        m_songSelectPage->showAnalysisError(
            QStringLiteral("音频文件已被修改或删除，请重新选择并分析"));
        m_cacheManager->remove(filePath);
        refreshHistory();
        return;
    }

    // ── 显示加载中状态 ──
    m_songSelectPage->showLoadingState();
    QApplication::processEvents();  // 立即刷新 UI

    // 加载音频文件（播放用）
    m_currentSongPath = filePath;
    bool loadOk = m_audioEngine->loadFile(filePath);
    if (!loadOk) {
        m_songSelectPage->hideLoadingState();
        m_songSelectPage->showAnalysisError(
            QStringLiteral("无法加载音频文件，请检查文件是否损坏"));
        return;
    }

    // 从缓存恢复音符数据
    m_currentNotes.clear();
    m_currentNotes.reserve(entry->notes.size());
    for (const auto& pair : entry->notes) {
        m_currentNotes.append(GameNote(pair.first, pair.second));
    }
    m_analyzedBpm = entry->bpm;

    // 更新 UI 为已分析状态
    m_songSelectPage->loadFromCache(filePath, m_analyzedBpm, entry->durationMs);
    m_songSelectPage->hideLoadingState();
}

void MainWindow::onHistoryDeleteRequested(const QString& filePath)
{
    m_cacheManager->remove(filePath);
    refreshHistory();
}

void MainWindow::saveToCache()
{
    if (m_currentSongPath.isEmpty() || m_currentNotes.isEmpty()) return;

    CacheEntry entry;
    entry.filePath = m_currentSongPath;
    QFileInfo fi(m_currentSongPath);
    entry.fileName = fi.fileName();
    entry.fileSize = fi.size();
    entry.durationMs = m_audioEngine->duration();
    entry.bpm = m_analyzedBpm;
    entry.analyzedAt = QDateTime::currentDateTime();

    entry.notes.reserve(m_currentNotes.size());
    for (const GameNote& note : m_currentNotes) {
        entry.notes.append({note.timestampMs, note.lane});
    }

    m_cacheManager->upsert(entry);
}

void MainWindow::refreshHistory()
{
    m_songSelectPage->refreshHistory(m_cacheManager->allEntries());
}

// ── 谱面编辑器相关 ───────────────────────────────────────────────
void MainWindow::onChartEditRequested()
{
    if (m_currentSongPath.isEmpty() || m_currentNotes.isEmpty()) return;

    QFileInfo fi(m_currentSongPath);
    qint64 fileSize = fi.size();
    qint64 duration = m_audioEngine->duration();

    // 优先载入已保存的谱面，没有则用当前分析结果
    QVector<GameNote> notes;
    const ChartEntry* chart = m_chartManager->findChart(fileSize);
    if (chart) {
        notes.reserve(chart->notes.size());
        for (const auto& pair : chart->notes) {
            notes.append(GameNote(pair.first, pair.second));
        }
    } else {
        notes = m_currentNotes;
    }

    m_chartEditorPage->loadChart(notes, m_currentSongPath, fileSize, duration, m_analyzedBpm);
    navigateTo(6);
}

void MainWindow::onChartPlayRequested(const QVector<GameNote>& notes)
{
    // 用编辑后的音符替换当前音符，复用现有游戏入口
    m_currentNotes = notes;
    m_audioEngine->pause();
    navigateTo(1);
    onGameRequested();
}
