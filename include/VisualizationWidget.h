#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QtMultimedia/QMediaPlayer>
#include "FFTAnalyzer.h"

class BarSpectrumVisualizer;
class CircularSpectrumVisualizer;
class WaveformVisualizer;
class WaterfallVisualizer;
class GLSpectrumWidget;
class AudioEffectWidget;
class AudioEffectProcessor;
class QPushButton;
class QSlider;
class QComboBox;
class QLabel;
class QTimer;
class AudioEngine;

class VisualizationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VisualizationWidget(AudioEngine* audioEngine, QWidget* parent = nullptr);
    ~VisualizationWidget() override;

    void startVisualization();
    void stopVisualization();

signals:
    void backRequested();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onRenderTick();
    void onPlayPauseClicked();
    void onSeekChanged(int value);
    void onVolumeChanged(int value);
    void onVisModeChanged(int index);
    void onFftSizeChanged(int index);
    void onPositionChanged(qint64 ms);
    void onDurationChanged(qint64 ms);

    /// 更新时间标签
    void updateTimeLabel(qint64 pos);

private:
    AudioEngine* m_audioEngine;
    FFTAnalyzer m_fftAnalyzer;
    QStackedWidget* m_visStack;
    BarSpectrumVisualizer* m_barWidget;
    CircularSpectrumVisualizer* m_circularWidget;
    WaveformVisualizer* m_waveformWidget;
    WaterfallVisualizer* m_waterfallWidget;
    GLSpectrumWidget* m_glWidget;
    AudioEffectWidget* m_audioEffectWidget;
    AudioEffectProcessor* m_effectProcessor;
    QPushButton* m_playPauseBtn;
    QPushButton* m_backBtn;
    QPushButton* m_fxBtn;               ///< FX 特效开关按钮
    QSlider* m_positionSlider;
    QSlider* m_volumeSlider;
    QSlider* m_compressionSlider;
    QLabel* m_compressionLabel;
    QComboBox* m_fftSizeCombo;
    QComboBox* m_visModeCombo;
    QLabel* m_timeLabel;
    QTimer* m_renderTimer;
    qint64 m_durationMs;
    float m_compressionPower;
    bool m_seeking;
    bool m_effectsActive = false;    ///< 音频特效是否已激活
};
