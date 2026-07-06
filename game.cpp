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
#include "ThemeManager.h"
#include "ThemeEditorWidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QEventLoop>
#include <QtConcurrent>
#include <QMessageBox>
#include <QDebug>
#include <QSet>

// ── 音符密度归一化工具：目标 ~100 音符/分钟 ──
// 密集歌曲多剔除 TAP，稀疏歌曲不剔除，HOLD 始终保留
static void normalizeNoteDensity(QVector<GameNote>& notes, qint64 durationMs)
{
    static constexpr int TARGET_NPM = 100;
    if (durationMs <= 0 || notes.size() <= 10) return;

    double minutes = durationMs / 60000.0;
    int targetCount = static_cast<int>(TARGET_NPM * minutes);
    if (targetCount < 30) targetCount = 30;

    if (notes.size() > targetCount) {
        int tapCount = 0;
        for (const GameNote& n : notes) {
            if (n.noteType != HOLD) ++tapCount;
        }
        int tapsToRemove = notes.size() - targetCount;
        if (tapsToRemove > 0 && tapsToRemove < tapCount) {
            int removeEvery = tapCount / tapsToRemove;
            if (removeEvery < 2) removeEvery = 2;

            QVector<GameNote> thinned;
            int ti = 0;
            for (const GameNote& note : notes) {
                if (note.noteType == HOLD) {
                    thinned.append(note);
                } else {
                    ++ti;
                    if (ti % removeEvery != 0) {
                        thinned.append(note);
                    }
                }
            }
            notes = thinned;
        }
    }
}

// ── Hold 音符合成工具：将合适位置的 TAP 转为 HOLD ──
static void mergeHolds(QVector<GameNote>& notes)
{
    static constexpr qint64 HOLD_MAX_DURATION = 3000;
    static constexpr qint64 HOLD_HEAD_BUFFER = 150;
    static constexpr qint64 HOLD_TAIL_BUFFER = 150;

    QVector<GameNote> result = notes;
    std::sort(result.begin(), result.end(), [](const GameNote& a, const GameNote& b) {
        return a.timestampMs < b.timestampMs;
    });

    // 估算中位间隔
    QVector<qint64> gaps;
    for (int i = 1; i < result.size(); ++i) {
        gaps.append(result[i].timestampMs - result[i - 1].timestampMs);
    }
    qint64 medianGap = 500;
    if (!gaps.isEmpty()) {
        std::sort(gaps.begin(), gaps.end());
        medianGap = gaps[gaps.size() / 2];
    }

    // 停顿 Hold 生成（减半频率）
    int pauseConvertCount = 0;
    for (int i = 0; i < result.size() - 1; ++i) {
        if (result[i].noteType != TAP) continue;
        qint64 gap = result[i + 1].timestampMs - result[i].timestampMs;
        if (gap > 2 * medianGap && gap < 6 * medianGap) {
            if (++pauseConvertCount % 2 == 0) continue;
            result[i].noteType = HOLD;
            // gap 在 [2x, 6x] medianGap 范围线性映射到 [500, 3000] ms
            double t = static_cast<double>(gap - 2 * medianGap)
                     / static_cast<double>(4 * medianGap);
            t = qBound(0.0, t, 1.0);
            result[i].holdDurationMs = static_cast<qint64>(500.0 + t * 2500.0);
        }
    }

    // 最低 Hold 比例保障（≥5%）
    int holdCount = 0;
    for (const GameNote& n : result) {
        if (n.noteType == HOLD) ++holdCount;
    }
    if (!result.isEmpty() && holdCount * 20 < result.size()) {
        int tapIndex = 0;
        for (int i = 0; i < result.size(); ++i) {
            if (result[i].noteType != TAP) continue;
            ++tapIndex;
            if (tapIndex % 14 == 0) {
                result[i].noteType = HOLD;
                // 填充 HOLD 也做长度变化：基于 tapIndex 产生不同时长
                double vary = static_cast<double>(tapIndex % 7) / 6.0;
                result[i].holdDurationMs = static_cast<qint64>(
                    600.0 + vary * (static_cast<double>(medianGap) * 4.0));
                result[i].holdDurationMs = qBound(qint64(500),
                    result[i].holdDurationMs, qint64(2500));
            }
        }
    }

    // 清理与 HOLD 重叠的音符
    {
        QVector<GameNote> cleaned;
        cleaned.reserve(result.size());
        for (int i = 0; i < result.size(); ++i) {
            const GameNote& note = result[i];
            bool swallowed = false;
            for (int j = 0; j < result.size(); ++j) {
                if (i == j) continue;
                const GameNote& hold = result[j];
                if (hold.noteType != HOLD) continue;
                if (hold.lane != note.lane) continue;
                qint64 holdStart = hold.timestampMs - HOLD_HEAD_BUFFER;
                qint64 holdEnd = hold.timestampMs + hold.holdDurationMs + HOLD_TAIL_BUFFER;
                if (note.timestampMs >= holdStart && note.timestampMs <= holdEnd) {
                    swallowed = true;
                    break;
                }
            }
            if (!swallowed) cleaned.append(note);
        }
        result = cleaned;
    }

    notes = result;
}

// ── 情绪分类：根据音频特征推断主题 ──
static Mood classifyMood(float bpm, float lowFreqRatio, float avgEnergy)
{
    // 激昂：高 BPM + 高能量
    if (bpm >= 150 && avgEnergy >= 0.5f) return Mood::Energetic;
    // 低沉：低 BPM + 低能量 + 低频主导
    if (bpm < 100 && avgEnergy < 0.4f && lowFreqRatio > 0.5f) return Mood::Melancholic;
    // 舒缓：中低 BPM + 低能量
    if (bpm < 130 && avgEnergy < 0.35f) return Mood::Calm;
    // 欢快：中高 BPM + 中等能量
    if (bpm >= 120 && avgEnergy >= 0.35f) return Mood::Cheerful;
    // 高能量但 BPM 不高也算激昂
    if (avgEnergy >= 0.6f && bpm >= 130) return Mood::Energetic;
    return Mood::Default;
}

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
    , m_themeEditorPage(nullptr)
    , m_loadWatcher(new QFutureWatcher<void>(this))
    , m_loadSuccess(false)
    , m_analyzedBpm(0.0f)
    , m_pendingLowFreqRatio(0.0f)
    , m_pendingAvgEnergy(0.0f)
    , m_analysisTimer(new QTimer(this))
    , m_analysisActive(false)
    , m_gameLaneCount(6)
{
    setupUI();
    connectSignals();

    // 清理缓存中已不存在的歌曲的排行榜孤儿数据
    {
        QSet<qint64> cachedSizes;
        for (const CacheEntry& e : m_cacheManager->allEntries())
            cachedSizes.insert(e.fileSize);

        QVector<LeaderboardEntry> allLb = m_leaderboardManager->allEntries();
        for (const LeaderboardEntry& le : allLb) {
            if (!cachedSizes.contains(le.songFileSize)) {
                m_leaderboardManager->clearSong(le.songFileSize, le.laneCount, false);
                m_leaderboardManager->clearSong(le.songFileSize, le.laneCount, true);
            }
        }
    }

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
    m_songSelectPage->setLeaderboardManager(m_leaderboardManager);
    m_visPage = new VisualizationWidget(m_audioEngine, this);
    m_gamePage = new GameWidget(m_audioEngine, m_scoreManager, this);
    m_resultPage = new ResultWidget(this);
    m_leaderboardPage = new LeaderboardWidget(m_leaderboardManager, this);
    m_chartEditorPage = new ChartEditorWidget(m_audioEngine, m_chartManager, this);
    m_themeEditorPage = new ThemeEditorWidget(this);

    // 添加到 QStackedWidget（按顺序：0-7）
    m_stack->addWidget(m_mainMenuPage);      // 索引 0：主菜单
    m_stack->addWidget(m_songSelectPage);    // 索引 1：歌曲选择
    m_stack->addWidget(m_visPage);           // 索引 2：可视化
    m_stack->addWidget(m_gamePage);          // 索引 3：游戏
    m_stack->addWidget(m_resultPage);        // 索引 4：结算
    m_stack->addWidget(m_leaderboardPage);   // 索引 5：排行榜
    m_stack->addWidget(m_chartEditorPage);   // 索引 6：谱面编辑
    m_stack->addWidget(m_themeEditorPage);   // 索引 7：主题编辑

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
    connect(m_mainMenuPage, &MainMenuWidget::themeEditRequested,
            this, &MainWindow::onThemeEditRequested);

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

    // 主题编辑器 → 返回主菜单
    connect(m_themeEditorPage, &ThemeEditorWidget::backRequested,
            this, [this]() { navigateTo(0); });

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
    connect(m_gamePage, &GameWidget::gameOver,
            this, &MainWindow::onSurvivalGameOver);
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
    if (pageIndex == 1) {
        m_songSelectPage->updateScoreHistory();
    }
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
        normalizeNoteDensity(m_pendingNotes, engine->duration());
        mergeHolds(m_pendingNotes);  // 分析阶段即生成 HOLD 音符
        m_analyzedBpm = detector->bpm();
        auto af = detector->audioFeatures();
        m_pendingLowFreqRatio = af.lowFreqRatio;
        m_pendingAvgEnergy = af.avgEnergy;
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

        // 根据音频特征自动切换游戏内主题（仅当开关开启且用户未手动选主题时）
        if (ThemeManager::instance()->autoMoodEnabled()
            && !ThemeManager::instance()->manualGameOverride()) {
            Mood mood = classifyMood(m_analyzedBpm, m_pendingLowFreqRatio, m_pendingAvgEnergy);
            ThemeManager::instance()->applyGameTheme(mood);
        }

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

    // 从歌曲选择页直接获取键数（4K/6K 已在选歌界面选择）
    m_gameLaneCount = m_songSelectPage->selectedLaneCount();

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
    m_survivalMode = m_songSelectPage->isSurvivalMode();
    m_gamePage->startGame(m_gameNotes, m_gameLaneCount, m_survivalMode);
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
    if (m_survivalMode)
        m_resultPage->setSurvivalResult(true);
    // 预填上次使用的玩家名，供玩家确认后记入排行榜
    m_resultPage->presetName(m_leaderboardManager->myName());
    navigateTo(4);
}

void MainWindow::onSurvivalGameOver()
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
    m_resultPage->setSurvivalResult(false);
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
    m_gamePage->startGame(m_gameNotes, m_gameLaneCount, m_survivalMode);
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
    entry.survival       = m_survivalMode;
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
    int total = m_leaderboardManager->entriesForSong(entry.songFileSize, entry.laneCount, m_survivalMode).size();
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
    m_currentSongPath = filePath;

    // 缓存entry数据（指针在异步回调时可能失效）
    float cachedBpm = entry->bpm;
    qint64 cachedDurationMs = entry->durationMs;
    QVector<QPair<qint64,int>> cachedNotes = entry->notes;
    float cachedLowFreqRatio = entry->lowFreqRatio;
    float cachedAvgEnergy = entry->avgEnergy;

    // 异步加载音频文件，不阻塞主线程（spinner可正常转圈）
    AudioEngine* engine = m_audioEngine;
    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, filePath,
            cachedBpm, cachedDurationMs, cachedNotes, cachedLowFreqRatio, cachedAvgEnergy]() {
        watcher->deleteLater();
        bool loadOk = watcher->result();
        if (!loadOk) {
            m_songSelectPage->hideLoadingState();
            m_songSelectPage->showAnalysisError(
                QStringLiteral("无法加载音频文件，请检查文件是否损坏"));
            return;
        }

        // 从缓存恢复音符数据
        m_currentNotes.clear();
        m_currentNotes.reserve(cachedNotes.size());
        for (const auto& pair : cachedNotes) {
            m_currentNotes.append(GameNote(pair.first, pair.second));
        }
        m_analyzedBpm = cachedBpm;
        m_pendingLowFreqRatio = cachedLowFreqRatio;
        m_pendingAvgEnergy = cachedAvgEnergy;
        mergeHolds(m_currentNotes);

        // 从缓存恢复时也应用对应游戏内主题（用户手动选主题时跳过）
        if (ThemeManager::instance()->autoMoodEnabled()
            && !ThemeManager::instance()->manualGameOverride()) {
            Mood mood = classifyMood(cachedBpm, cachedLowFreqRatio, cachedAvgEnergy);
            ThemeManager::instance()->applyGameTheme(mood);
        }

        // 更新 UI 为已分析状态
        m_songSelectPage->loadFromCache(filePath, m_analyzedBpm, cachedDurationMs, cachedNotes);
        m_songSelectPage->hideLoadingState();
    });

    auto future = QtConcurrent::run([engine, filePath]() -> bool {
        return engine->loadFile(filePath);
    });
    watcher->setFuture(future);
}

void MainWindow::onHistoryDeleteRequested(const QString& filePath)
{
    m_cacheManager->remove(filePath);

    // 同步删除排行榜数据（4K + 6K）
    QFileInfo fi(filePath);
    qint64 fileSize = fi.size();
    if (fileSize > 0) {
        m_leaderboardManager->clearSong(fileSize, 4, false);
        m_leaderboardManager->clearSong(fileSize, 6, false);
        m_leaderboardManager->clearSong(fileSize, 4, true);
        m_leaderboardManager->clearSong(fileSize, 6, true);
    }

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
    entry.lowFreqRatio = m_pendingLowFreqRatio;
    entry.avgEnergy = m_pendingAvgEnergy;
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
        notes = chart->notes;
    } else {
        notes = m_currentNotes;
    }

    // 如果谱面中没有任何 HOLD 音符（旧谱面 / v1 格式 / 缓存重建），补跑 mergeHolds
    bool hasHolds = false;
    for (const GameNote& n : notes) {
        if (n.noteType == HOLD) { hasHolds = true; break; }
    }
    if (!hasHolds && !notes.isEmpty()) {
        mergeHolds(notes);
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

void MainWindow::onThemeEditRequested()
{
    navigateTo(7);
}
