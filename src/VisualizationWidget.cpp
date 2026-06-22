#include "VisualizationWidget.h"
#include "AudioEngine.h"
#include "FFTAnalyzer.h"
#include "SpectrumProcessor.h"
#include "BarSpectrumVisualizer.h"
#include "CircularSpectrumVisualizer.h"
#include "WaveformVisualizer.h"
#include "WaterfallVisualizer.h"
#include "GLSpectrumWidget.h"
#include "AudioEffectWidget.h"
#include "AudioEffectProcessor.h"

#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QtMultimedia/QMediaPlayer>

VisualizationWidget::VisualizationWidget(AudioEngine* audioEngine, QWidget* parent)
    : QWidget(parent)
    , m_audioEngine(audioEngine)
    , m_fftAnalyzer(2048)
    , m_durationMs(0)
    , m_compressionPower(0.33f)
    , m_seeking(false)
    , m_effectProcessor(new AudioEffectProcessor)
    , m_renderTimer(new QTimer(this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(10, 10, 10, 10);

    // ── 可视化组件堆栈 ──
    m_visStack = new QStackedWidget(this);
    m_barWidget = new BarSpectrumVisualizer(this);
    m_circularWidget = new CircularSpectrumVisualizer(this);
    m_waveformWidget = new WaveformVisualizer(this);
    m_waterfallWidget = new WaterfallVisualizer(this);
    m_glWidget = new GLSpectrumWidget(this);
    m_visStack->addWidget(m_barWidget);
    m_visStack->addWidget(m_circularWidget);
    m_visStack->addWidget(m_waveformWidget);
    m_visStack->addWidget(m_waterfallWidget);
    m_visStack->addWidget(m_glWidget);
    layout->addWidget(m_visStack, 1);

    // ── 音频特效浮层（覆盖在频谱上方）──
    m_audioEffectWidget = new AudioEffectWidget(m_effectProcessor, this);
    m_audioEffectWidget->hide();

    // 底部控制面板
    QWidget* controlPanel = new QWidget(this);
    controlPanel->setObjectName("controlPanel");
    controlPanel->setFixedHeight(80);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setSpacing(4);
    controlLayout->setContentsMargins(8, 4, 8, 4);

    // 进度条行
    QHBoxLayout* seekLayout = new QHBoxLayout();
    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), this);
    m_timeLabel->setStyleSheet("color: #aaaaaa; background: transparent;");
    m_timeLabel->setMinimumWidth(120);
    seekLayout->addWidget(m_timeLabel);

    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setRange(0, 1000);
    m_positionSlider->setValue(0);
    seekLayout->addWidget(m_positionSlider, 1);
    controlLayout->addLayout(seekLayout);

    // 按钮行
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_playPauseBtn = new QPushButton(QStringLiteral("播放"), this);
    m_playPauseBtn->setFixedWidth(70);
    m_playPauseBtn->setObjectName("playButton");
    btnLayout->addWidget(m_playPauseBtn);

    QLabel* volLabel = new QLabel(QStringLiteral("音量"), this);
    volLabel->setStyleSheet("color: #aaaaaa; background: transparent;");
    btnLayout->addWidget(volLabel);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100); m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(100);
    btnLayout->addWidget(m_volumeSlider);

    QLabel* fftLabel = new QLabel(QStringLiteral("FFT"), this);
    fftLabel->setStyleSheet("color: #aaaaaa; background: transparent;");
    btnLayout->addWidget(fftLabel);

    m_fftSizeCombo = new QComboBox(this);
    m_fftSizeCombo->addItems({"512", "1024", "2048"});
    m_fftSizeCombo->setCurrentIndex(2);
    m_fftSizeCombo->setFixedWidth(80);
    btnLayout->addWidget(m_fftSizeCombo);

    QLabel* visLabel = new QLabel(QStringLiteral("模式"), this);
    visLabel->setStyleSheet("color: #aaaaaa; background: transparent;");
    btnLayout->addWidget(visLabel);

    m_visModeCombo = new QComboBox(this);
    m_visModeCombo->addItems({QStringLiteral("柱状频谱"), QStringLiteral("圆形频谱"),
                              QStringLiteral("波形"), QStringLiteral("瀑布图"),
                              QStringLiteral("GL 霓虹"), QStringLiteral("GL 圆形")});
    m_visModeCombo->setFixedWidth(100);
    btnLayout->addWidget(m_visModeCombo);

    // ── FX 按钮（音频特效开关）──
    m_fxBtn = new QPushButton(QStringLiteral("FX"), this);
    m_fxBtn->setFixedSize(40, 28);
    m_fxBtn->setCheckable(true);
    m_fxBtn->setStyleSheet(
        "QPushButton { font-size: 12px; font-weight: bold; color: #00ff88; "
        "  background-color: #16213e; border: 1px solid #00ff88; border-radius: 4px; }"
        "QPushButton:hover { background-color: #00ff88; color: #0a0a1e; }"
        "QPushButton:checked { background-color: #00ff88; color: #0a0a1e; }");
    btnLayout->addWidget(m_fxBtn);

    connect(m_fxBtn, &QPushButton::toggled, this, [this](bool on) {
        if (on) {
            // 开启实时特效播放（暂停 QMediaPlayer，启动 QAudioSink）
            if (m_audioEngine && m_audioEngine->isLoaded()) {
                m_effectProcessor->resetState();
                m_audioEngine->startRealtimePlayback(m_effectProcessor);
                m_playPauseBtn->setText(QStringLiteral("暂停"));
            }
            m_audioEffectWidget->show();
            m_audioEffectWidget->raise();
            m_effectsActive = true;
        } else {
            m_audioEngine->stopRealtimePlayback();
            m_audioEffectWidget->hide();
            m_effectsActive = false;
            m_playPauseBtn->setText(QStringLiteral("播放"));
        }
    });

    // 面板内部关闭按钮 → 取消 FX
    connect(m_audioEffectWidget, &AudioEffectWidget::requestClose, this, [this]() {
        m_fxBtn->setChecked(false);
    });

    // 参数变化 → processor 已实时更新，AudioOutputDevice 下一帧自动生效
    connect(m_audioEffectWidget, &AudioEffectWidget::effectChanged, this, []() {});

    // 压缩强度滑块
    QLabel* compLabel = new QLabel(QStringLiteral("压缩"), this);
    compLabel->setStyleSheet("color: #aaaaaa; background: transparent; font-size: 12px;");
    btnLayout->addWidget(compLabel);

    m_compressionSlider = new QSlider(Qt::Horizontal, this);
    m_compressionSlider->setRange(5, 100);
    m_compressionSlider->setValue(static_cast<int>(m_compressionPower * 100));
    m_compressionSlider->setFixedWidth(80);
    btnLayout->addWidget(m_compressionSlider);

    m_compressionLabel = new QLabel(QStringLiteral(".33"), this);
    m_compressionLabel->setMinimumWidth(30);
    m_compressionLabel->setStyleSheet("color: #e0e0e0; background: transparent; font-size: 12px;");
    btnLayout->addWidget(m_compressionLabel);

    connect(m_compressionSlider, &QSlider::valueChanged, this, [this](int v) {
        m_compressionPower = v / 100.0f;
        m_compressionLabel->setText(QStringLiteral(".%1").arg(v));
    });

    btnLayout->addStretch();

    m_backBtn = new QPushButton(QStringLiteral("返回"), this);
    m_backBtn->setFixedWidth(70);
    m_backBtn->setObjectName("backButton");
    btnLayout->addWidget(m_backBtn);

    controlLayout->addLayout(btnLayout);
    layout->addWidget(controlPanel);

    // 信号连接
    connect(m_playPauseBtn, &QPushButton::clicked, this, &VisualizationWidget::onPlayPauseClicked);
    connect(m_positionSlider, &QSlider::sliderPressed, this, [this]() { m_seeking = true; });
    connect(m_positionSlider, &QSlider::sliderReleased, this, [this]() {
        m_seeking = false;
        onSeekChanged(m_positionSlider->value());
    });
    connect(m_volumeSlider, &QSlider::valueChanged, this, &VisualizationWidget::onVolumeChanged);
    connect(m_fftSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VisualizationWidget::onFftSizeChanged);
    connect(m_visModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VisualizationWidget::onVisModeChanged);
    m_renderTimer->setTimerType(Qt::PreciseTimer);
    m_renderTimer->setInterval(16);  // ~60fps
    connect(m_renderTimer, &QTimer::timeout, this, &VisualizationWidget::onRenderTick);

    connect(m_backBtn, &QPushButton::clicked, this, [this]() {
        m_fxBtn->setChecked(false);  // 触发 stopRealtimePlayback + hide panel
        emit backRequested();
    });

    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::positionChanged, this, &VisualizationWidget::onPositionChanged);
        connect(m_audioEngine, &AudioEngine::durationChanged, this, &VisualizationWidget::onDurationChanged);
    }
}

VisualizationWidget::~VisualizationWidget()
{
    m_audioEngine->stopRealtimePlayback();
    delete m_effectProcessor;
}

void VisualizationWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // 浮层面板定位到右上角
    if (m_audioEffectWidget) {
        m_audioEffectWidget->move(width() - m_audioEffectWidget->width() - 10, 10);
        m_audioEffectWidget->raise();
    }
}

void VisualizationWidget::startVisualization()
{
    if (m_audioEngine && m_audioEngine->isLoaded()) {
        m_effectProcessor->setSource(m_audioEngine->pcmData(), m_audioEngine->sampleRate());
    }
    m_renderTimer->start();
}

void VisualizationWidget::stopVisualization()
{
    m_renderTimer->stop();
    m_fxBtn->setChecked(false);
    m_audioEngine->stopRealtimePlayback();
    m_effectsActive = false;
}

void VisualizationWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
}

void VisualizationWidget::onRenderTick()
{
    if (!m_audioEngine || !m_audioEngine->isLoaded()) return;

    qint64 pos = m_audioEngine->position();

    // 只在有实际数据时做 FFT
    int fftSize = m_fftAnalyzer.windowSize();
    QVector<float> window = m_audioEngine->getWindowAt(pos, fftSize);

    // FFT
    QVector<float> magnitude;
    m_fftAnalyzer.compute(window, magnitude);

    // 频谱后处理
    int barCount = 128;
    QVector<float> processed = SpectrumProcessor::process(
        magnitude, m_audioEngine->sampleRate(), fftSize, barCount, m_compressionPower);

    int visIdx = m_visStack->currentIndex();
    if (visIdx == 0) m_barWidget->setSpectrumData(processed);
    else if (visIdx == 1) m_circularWidget->setSpectrumData(processed);
    else if (visIdx == 2) m_waveformWidget->setWaveformData(window);
    else if (visIdx == 3) m_waterfallWidget->setSpectrumData(processed);
    else if (visIdx == 4) m_glWidget->setSpectrumData(processed);

    // 进度条 + 时间标签
    if (!m_seeking && m_durationMs > 0) {
        m_positionSlider->setValue(static_cast<int>(pos * 1000.0 / m_durationMs));
    }
    updateTimeLabel(pos);
}

void VisualizationWidget::onPlayPauseClicked()
{
    if (!m_audioEngine) return;
    if (m_effectsActive) {
        // 实时特效模式
        if (m_audioEngine->isRealtimePlaying()) {
            m_audioEngine->pause();
            m_playPauseBtn->setText(QStringLiteral("播放"));
        } else {
            m_audioEngine->play();
            m_playPauseBtn->setText(QStringLiteral("暂停"));
        }
    } else {
        // 普通模式
        if (m_audioEngine->state() == QMediaPlayer::PlayingState) {
            m_audioEngine->pause();
            m_playPauseBtn->setText(QStringLiteral("播放"));
        } else {
            m_audioEngine->play();
            m_playPauseBtn->setText(QStringLiteral("暂停"));
        }
    }
}

void VisualizationWidget::onSeekChanged(int value)
{
    if (m_durationMs <= 0) return;
    qint64 ms = static_cast<qint64>(value) * m_durationMs / 1000;
    m_audioEngine->seek(ms);
    if (m_effectsActive) {
        m_effectProcessor->resetState();
    }
}

void VisualizationWidget::onVolumeChanged(int value)
{
    m_audioEngine->setVolume(value / 100.0f);
}

void VisualizationWidget::onVisModeChanged(int index)
{
    if (index == 5) {
        m_glWidget->setRenderMode(GLSpectrumWidget::Circular);
        m_visStack->setCurrentIndex(4);
    } else {
        if (index == 4) m_glWidget->setRenderMode(GLSpectrumWidget::Bars);
        m_visStack->setCurrentIndex(index);
    }
}

void VisualizationWidget::onFftSizeChanged(int index)
{
    int sizes[] = { 512, 1024, 2048 };
    if (index >= 0 && index < 3) m_fftAnalyzer.setWindowSize(sizes[index]);
}

void VisualizationWidget::onPositionChanged(qint64 ms)
{
    // realtime 模式下，onRenderTick 已经在更新进度条和时间，这里跳过避免重复
    if (m_effectsActive) return;
    if (!m_seeking && m_durationMs > 0) {
        m_positionSlider->setValue(static_cast<int>(ms * 1000.0 / m_durationMs));
    }
    updateTimeLabel(ms);
}

void VisualizationWidget::onDurationChanged(qint64 ms)
{
    m_durationMs = ms;
}

static QString msToTime(qint64 ms)
{
    int sec = static_cast<int>(ms / 1000);
    return QStringLiteral("%1:%2").arg(sec / 60).arg(sec % 60, 2, 10, QLatin1Char('0'));
}

void VisualizationWidget::updateTimeLabel(qint64 pos)
{
    m_timeLabel->setText(QStringLiteral("%1 / %2").arg(msToTime(pos), msToTime(m_durationMs)));
}
