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

#include <QApplication>
#include <QCloseEvent>
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
    , m_mainMenuPage(nullptr)
    , m_songSelectPage(nullptr)
    , m_visPage(nullptr)
    , m_gamePage(nullptr)
    , m_resultPage(nullptr)
    , m_loadWatcher(new QFutureWatcher<void>(this))
    , m_loadSuccess(false)
    , m_analyzedBpm(0.0f)
    , m_analysisTimer(new QTimer(this))
    , m_cancelAnalysis(new volatile bool(false))
    , m_analysisActive(false)
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
    *m_cancelAnalysis = true;
    m_analysisTimer->stop();
    delete m_noteGenerator;
    delete m_cancelAnalysis;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    *m_cancelAnalysis = true;
    m_analysisTimer->stop();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("SpectrumFall"));
    setMinimumSize(800, 600);
    resize(1000, 700);

    // 创建 5 个页面
    m_mainMenuPage = new MainMenuWidget(this);
    m_songSelectPage = new SongSelectWidget(this);
    m_visPage = new VisualizationWidget(m_audioEngine, this);
    m_gamePage = new GameWidget(m_audioEngine, m_scoreManager, this);
    m_resultPage = new ResultWidget(this);

    // 添加到 QStackedWidget（按顺序：0-4）
    m_stack->addWidget(m_mainMenuPage);      // 索引 0：主菜单
    m_stack->addWidget(m_songSelectPage);    // 索引 1：歌曲选择
    m_stack->addWidget(m_visPage);           // 索引 2：可视化
    m_stack->addWidget(m_gamePage);          // 索引 3：游戏
    m_stack->addWidget(m_resultPage);        // 索引 4：结算

    setCentralWidget(m_stack);
}

void MainWindow::connectSignals()
{
    // 主菜单 → 歌曲选择
    connect(m_mainMenuPage, &MainMenuWidget::songSelectRequested,
            this, &MainWindow::onSongSelectRequested);
    connect(m_mainMenuPage, &MainMenuWidget::exitRequested,
            QApplication::instance(), &QApplication::quit);

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

    // 结算页 → 重试/返回
    connect(m_resultPage, &ResultWidget::retryRequested,
            this, &MainWindow::onRetryRequested);
    connect(m_resultPage, &ResultWidget::backRequested,
            this, &MainWindow::onBackToMenuRequested);

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

void MainWindow::onAnalyzeRequested(const QString& path)
{
    // 防止重复点击
    if (m_analysisActive) return;

    m_currentSongPath = path;
    m_loadSuccess = false;
    m_analyzedBpm = 0.0f;
    m_errorMessage.clear();
    m_pendingNotes.clear();
    *m_cancelAnalysis = false;
    m_analysisActive = true;

    // 启动 90 秒超时定时器
    m_analysisTimer->start(90000);

    AudioEngine* engine = m_audioEngine;
    BeatDetector* detector = m_beatDetector;
    NoteGenerator* generator = m_noteGenerator;

    auto future = QtConcurrent::run([this, engine, detector, generator, path]() {
        // ── Phase 1: 加载音频文件（0% → 30%）──
        emit analysisProgressChanged(5);

        m_loadSuccess = engine->loadFile(path);

        if (!m_loadSuccess) {
            if (path.endsWith(".flac", Qt::CaseInsensitive)) {
                m_errorMessage = QStringLiteral("FLAC 格式暂不支持节拍分析，请选择 MP3 或 WAV 文件");
            } else {
                m_errorMessage = QStringLiteral("无法加载音频文件，请检查文件是否损坏或格式不受支持");
            }
            return;
        }

        if (*m_cancelAnalysis) return;

        // 更新时长 + 进度
        emit durationUpdated(engine->duration());
        emit analysisProgressChanged(30);

        // 检查音频是否太短
        if (engine->duration() < 10000) {
            m_loadSuccess = false;
            m_errorMessage = QStringLiteral("音频太短（不足 10 秒），无法生成谱面");
            return;
        }

        if (*m_cancelAnalysis) return;

        // ── Phase 2: 节拍检测（30% → 85%）──
        const QVector<float>& pcm = engine->pcmData();

        auto progressCb = [this](int percent) {
            int overall = 30 + percent * 55 / 100;
            emit analysisProgressChanged(overall);
        };

        detector->analyze(pcm, engine->sampleRate(), progressCb, m_cancelAnalysis);

        if (*m_cancelAnalysis) return;

        emit analysisProgressChanged(90);

        // ── Phase 3: 生成谱面（90% → 100%）──
        m_pendingNotes = generator->generate(detector->beatPoints());
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

    if (*m_cancelAnalysis) {
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
    *m_cancelAnalysis = true;
    m_analysisActive = false;
    m_songSelectPage->showAnalysisError(
        QStringLiteral("分析超时（90 秒），请尝试其他歌曲或更短的音频"));
}

void MainWindow::cancelAnalysis()
{
    if (m_analysisActive) {
        *m_cancelAnalysis = true;
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

    navigateTo(3);
    m_gamePage->startGame(m_currentNotes);
}

void MainWindow::onGameFinished()
{
    int totalNotes = m_currentNotes.size();
    m_resultPage->setResult(
        m_scoreManager->score(),
        m_scoreManager->perfectCount(),
        m_scoreManager->goodCount(),
        m_scoreManager->missCount(),
        m_scoreManager->maxCombo(),
        totalNotes
    );
    navigateTo(4);
}

void MainWindow::onRetryRequested()
{
    m_scoreManager->reset();
    if (m_currentNotes.isEmpty()) {
        navigateTo(1);
        return;
    }
    navigateTo(3);
    m_gamePage->startGame(m_currentNotes);
}

void MainWindow::onBackToMenuRequested()
{
    m_audioEngine->pause();
    navigateTo(0);
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
        // 删除无效缓存
        m_cacheManager->remove(filePath);
        refreshHistory();
        return;
    }

    // 加载音频文件（播放用）
    m_currentSongPath = filePath;
    bool loadOk = m_audioEngine->loadFile(filePath);
    if (!loadOk) {
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

    // 更新 UI 为已分析状态（使用公开方法，不直接访问私有成员）
    m_songSelectPage->loadFromCache(filePath, m_analyzedBpm, entry->durationMs);
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
