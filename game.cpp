#include "game.h"
#include "AudioEngine.h"
#include "BeatDetector.h"
#include "NoteGenerator.h"
#include "ScoreManager.h"
#include "MainMenuWidget.h"
#include "SongSelectWidget.h"
#include "VisualizationWidget.h"
#include "GameWidget.h"
#include "ResultWidget.h"

#include <QApplication>
#include <QtConcurrent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_stack(new QStackedWidget(this))
    , m_audioEngine(new AudioEngine(this))
    , m_beatDetector(new BeatDetector(this))
    , m_noteGenerator(new NoteGenerator())
    , m_scoreManager(new ScoreManager(this))
    , m_mainMenuPage(nullptr)
    , m_songSelectPage(nullptr)
    , m_visPage(nullptr)
    , m_gamePage(nullptr)
    , m_resultPage(nullptr)
    , m_loadWatcher(new QFutureWatcher<void>(this))
    , m_loadSuccess(false)
    , m_analyzedBpm(0.0f)
{
    setupUI();
    connectSignals();
    navigateTo(0); // 启动时显示主菜单
}

MainWindow::~MainWindow()
{
    delete m_noteGenerator;
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("Rhythm Wave"));
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

    // 歌曲选择页扫描默认目录
    m_songSelectPage->scanDirectory(QStringLiteral("E:/C++homework"));
}

void MainWindow::connectSignals()
{
    // 主菜单 → 歌曲选择
    connect(m_mainMenuPage, &MainMenuWidget::songSelectRequested,
            this, &MainWindow::onSongSelectRequested);
    connect(m_mainMenuPage, &MainMenuWidget::exitRequested,
            QApplication::instance(), &QApplication::quit);

    // 歌曲选择 → 各页面
    connect(m_songSelectPage, &SongSelectWidget::songSelected,
            this, &MainWindow::onSongSelected);
    connect(m_songSelectPage, &SongSelectWidget::visualizeRequested,
            this, &MainWindow::onVisualizeRequested);
    connect(m_songSelectPage, &SongSelectWidget::gameRequested,
            this, &MainWindow::onGameRequested);
    connect(m_songSelectPage, &SongSelectWidget::backRequested,
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
    connect(m_gamePage, &GameWidget::backRequested,
            this, [this]() { navigateTo(0); });

    // 结算页 → 重试/返回
    connect(m_resultPage, &ResultWidget::retryRequested,
            this, &MainWindow::onRetryRequested);
    connect(m_resultPage, &ResultWidget::backRequested,
            this, &MainWindow::onBackToMenuRequested);

    // 异步加载完成
    connect(m_loadWatcher, &QFutureWatcher<void>::finished,
            this, &MainWindow::onLoadAndAnalyzeFinished);
}

void MainWindow::navigateTo(int pageIndex)
{
    m_stack->setCurrentIndex(pageIndex);
}

void MainWindow::onSongSelectRequested()
{
    navigateTo(1);
}

void MainWindow::onSongSelected(const QString& path)
{
    m_currentSongPath = path;
    m_pendingSongPath = path;

    // 显示加载中提示
    m_songSelectPage->setBpmDisplay(-2.0f); // 用 -2 表示"加载中"

    // 异步加载音频 + 节拍分析（不阻塞 UI）
    AudioEngine* engine = m_audioEngine;
    BeatDetector* detector = m_beatDetector;
    QString songPath = path;

    auto future = QtConcurrent::run([engine, detector, songPath, this]() {
        // 在后台线程加载和解析 WAV
        m_loadSuccess = engine->loadFile(songPath);
        if (m_loadSuccess) {
            const QVector<float>& pcm = engine->pcmData();
            detector->analyze(pcm, engine->sampleRate());
            m_analyzedBpm = detector->bpm();
        }
    });

    m_loadWatcher->setFuture(future);
}

void MainWindow::onLoadAndAnalyzeFinished()
{
    if (m_loadSuccess) {
        m_songSelectPage->setBpmDisplay(m_analyzedBpm);
    } else {
        m_songSelectPage->setBpmDisplay(-1.0f);
    }
}

void MainWindow::onVisualizeRequested()
{
    if (m_currentSongPath.isEmpty()) return;

    // 如果正在加载，忽略
    if (m_loadWatcher->isRunning()) return;

    navigateTo(2);
    m_audioEngine->seek(0);
    m_audioEngine->play();
    m_visPage->startVisualization();
}

void MainWindow::onGameRequested()
{
    if (m_currentSongPath.isEmpty()) return;

    // 如果正在加载，忽略
    if (m_loadWatcher->isRunning()) return;

    // 从节拍检测结果生成音符
    const QVector<BeatPoint>& beats = m_beatDetector->beatPoints();
    m_currentNotes = m_noteGenerator->generate(beats);

    // 进入游戏页面
    navigateTo(3);
    m_gamePage->startGame(m_currentNotes);
}

void MainWindow::onGameFinished()
{
    // 显示结算页
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
    // 重新开始游戏
    m_scoreManager->reset();
    navigateTo(3);
    m_gamePage->startGame(m_currentNotes);
}

void MainWindow::onBackToMenuRequested()
{
    m_audioEngine->pause();
    navigateTo(0);
}
