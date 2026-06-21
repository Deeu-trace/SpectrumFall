#include "VisualizationWidget.h"
#include "AudioEngine.h"
#include "FFTAnalyzer.h"
#include "SpectrumProcessor.h"
#include "BarSpectrumVisualizer.h"
#include "CircularSpectrumVisualizer.h"
#include "WaveformVisualizer.h"
#include "WaterfallVisualizer.h"

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
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(10, 10, 10, 10);

    // 可视化组件堆栈
    m_visStack = new QStackedWidget(this);

    m_barWidget = new BarSpectrumVisualizer(this);
    m_circularWidget = new CircularSpectrumVisualizer(this);
    m_waveformWidget = new WaveformVisualizer(this);
    m_waterfallWidget = new WaterfallVisualizer(this);

    m_visStack->addWidget(m_barWidget);       // 索引 0
    m_visStack->addWidget(m_circularWidget);   // 索引 1
    m_visStack->addWidget(m_waveformWidget);   // 索引 2
    m_visStack->addWidget(m_waterfallWidget);  // 索引 3

    layout->addWidget(m_visStack, 1);

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
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(100);
    btnLayout->addWidget(m_volumeSlider);

    QLabel* fftLabel = new QLabel(QStringLiteral("FFT"), this);
    fftLabel->setStyleSheet("color: #aaaaaa; background: transparent;");
    btnLayout->addWidget(fftLabel);

    m_fftSizeCombo = new QComboBox(this);
    m_fftSizeCombo->addItems({"512", "1024", "2048"});
    m_fftSizeCombo->setCurrentIndex(2); // 默认 2048
    m_fftSizeCombo->setFixedWidth(80);
    btnLayout->addWidget(m_fftSizeCombo);

    QLabel* visLabel = new QLabel(QStringLiteral("模式"), this);
    visLabel->setStyleSheet("color: #aaaaaa; background: transparent;");
    btnLayout->addWidget(visLabel);

    m_visModeCombo = new QComboBox(this);
    m_visModeCombo->addItems({QStringLiteral("柱状频谱"), QStringLiteral("圆形频谱"),
                              QStringLiteral("波形"), QStringLiteral("瀑布图")});
    m_visModeCombo->setFixedWidth(100);
    btnLayout->addWidget(m_visModeCombo);

    // ── 压缩强度滑块 ──
    QLabel* compLabel = new QLabel(QStringLiteral("压缩"), this);
    compLabel->setStyleSheet("color: #aaaaaa; background: transparent; font-size: 12px;");
    btnLayout->addWidget(compLabel);

    m_compressionSlider = new QSlider(Qt::Horizontal, this);
    m_compressionSlider->setRange(5, 100);       // 0.05 ~ 1.00
    m_compressionSlider->setValue(static_cast<int>(m_compressionPower * 100));
    m_compressionSlider->setFixedWidth(80);
    m_compressionSlider->setObjectName("densitySlider");
    btnLayout->addWidget(m_compressionSlider);

    m_compressionLabel = new QLabel(QStringLiteral(".33"), this);
    m_compressionLabel->setMinimumWidth(30);
    m_compressionLabel->setStyleSheet("color: #e0e0e0; background: transparent; font-size: 12px;");
    btnLayout->addWidget(m_compressionLabel);

    connect(m_compressionSlider, &QSlider::valueChanged, this, [this](int value) {
        m_compressionPower = value / 100.0f;
        m_compressionLabel->setText(QStringLiteral(".%1").arg(value));
    });

    btnLayout->addStretch();

    m_backBtn = new QPushButton(QStringLiteral("返回"), this);
    m_backBtn->setFixedWidth(70);
    m_backBtn->setObjectName("backButton");
    btnLayout->addWidget(m_backBtn);

    controlLayout->addLayout(btnLayout);
    layout->addWidget(controlPanel);

    // 渲染定时器
    m_renderTimer = new QTimer(this);
    m_renderTimer->setInterval(16); // ~60Hz

    // 连接信号
    connect(m_playPauseBtn, &QPushButton::clicked, this, &VisualizationWidget::onPlayPauseClicked);
    connect(m_positionSlider, &QSlider::sliderPressed, this, [this]() { m_seeking = true; });
    connect(m_positionSlider, &QSlider::sliderReleased, this, [this]() { m_seeking = false; });
    connect(m_positionSlider, &QSlider::valueChanged, this, &VisualizationWidget::onSeekChanged);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &VisualizationWidget::onVolumeChanged);
    connect(m_visModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VisualizationWidget::onVisModeChanged);
    connect(m_fftSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VisualizationWidget::onFftSizeChanged);
    connect(m_renderTimer, &QTimer::timeout, this, &VisualizationWidget::onRenderTick);
    connect(m_backBtn, &QPushButton::clicked, this, &VisualizationWidget::backRequested);

    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::positionChanged, this, &VisualizationWidget::onPositionChanged);
        connect(m_audioEngine, &AudioEngine::durationChanged, this, &VisualizationWidget::onDurationChanged);
    }
}

void VisualizationWidget::startVisualization()
{
    m_renderTimer->start();
}

void VisualizationWidget::stopVisualization()
{
    m_renderTimer->stop();
}

void VisualizationWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
}

void VisualizationWidget::onRenderTick()
{
    if (!m_audioEngine || !m_audioEngine->isLoaded()) return;

    qint64 pos = m_audioEngine->position();
    int fftSize = m_fftAnalyzer.windowSize();

    // 获取当前位置的 PCM 窗口
    QVector<float> window = m_audioEngine->getWindowAt(pos, fftSize);

    // 计算 FFT
    QVector<float> magnitude;
    m_fftAnalyzer.compute(window, magnitude);

    // 频谱后处理：对数频率轴 + 频带均值 + 可调压缩强度
    QVector<float> processed = SpectrumProcessor::process(
        magnitude, m_audioEngine->sampleRate(), fftSize, 256, m_compressionPower);

    // 分发到当前活跃的可视化组件
    int visIndex = m_visStack->currentIndex();

    if (visIndex == 0) {
        // 柱状频谱
        m_barWidget->setSpectrumData(processed);
    } else if (visIndex == 1) {
        // 圆形频谱
        m_circularWidget->setSpectrumData(processed);
    } else if (visIndex == 2) {
        // 波形
        m_waveformWidget->setWaveformData(window);
    } else if (visIndex == 3) {
        // 瀑布图
        m_waterfallWidget->setSpectrumData(processed);
    }
}

void VisualizationWidget::onPlayPauseClicked()
{
    if (!m_audioEngine) return;

    if (m_audioEngine->state() == QMediaPlayer::PlayingState) {
        m_audioEngine->pause();
        m_playPauseBtn->setText(QStringLiteral("播放"));
    } else {
        m_audioEngine->play();
        m_playPauseBtn->setText(QStringLiteral("暂停"));
    }
}

void VisualizationWidget::onSeekChanged(int value)
{
    if (!m_audioEngine || !m_seeking) return;

    qint64 seekPos = static_cast<qint64>(value) * m_durationMs / 1000;
    m_audioEngine->seek(seekPos);
}

void VisualizationWidget::onVolumeChanged(int value)
{
    if (m_audioEngine) {
        m_audioEngine->setVolume(static_cast<float>(value) / 100.0f);
    }
}

void VisualizationWidget::onVisModeChanged(int index)
{
    m_visStack->setCurrentIndex(index);
}

void VisualizationWidget::onFftSizeChanged(int index)
{
    int sizes[] = {512, 1024, 2048};
    if (index >= 0 && index < 3) {
        m_fftAnalyzer.setWindowSize(sizes[index]);
    }
}

void VisualizationWidget::onPositionChanged(qint64 ms)
{
    if (m_seeking) return;

    // 更新进度条
    if (m_durationMs > 0) {
        m_positionSlider->blockSignals(true);
        m_positionSlider->setValue(static_cast<int>(ms * 1000 / m_durationMs));
        m_positionSlider->blockSignals(false);
    }

    // 更新时间标签
    qint64 sec = ms / 1000;
    qint64 totalSec = m_durationMs / 1000;
    m_timeLabel->setText(QStringLiteral("%1:%2 / %3:%4")
        .arg(sec / 60)
        .arg(sec % 60, 2, 10, QLatin1Char('0'))
        .arg(totalSec / 60)
        .arg(totalSec % 60, 2, 10, QLatin1Char('0')));
}

void VisualizationWidget::onDurationChanged(qint64 ms)
{
    m_durationMs = ms;
}
