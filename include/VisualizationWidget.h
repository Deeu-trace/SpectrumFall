#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QtMultimedia/QMediaPlayer>
#include "FFTAnalyzer.h"

class BarSpectrumVisualizer;
class CircularSpectrumVisualizer;
class WaveformVisualizer;
class WaterfallVisualizer;
class QPushButton;
class QSlider;
class QComboBox;
class QLabel;
class QTimer;
class AudioEngine;

/// 可视化页面：中央可视化区域 + 底部播放控制 + 模式切换标签
class VisualizationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VisualizationWidget(AudioEngine* audioEngine, QWidget* parent = nullptr);

    /// 开始可视化渲染循环
    void startVisualization();

    /// 停止可视化渲染循环
    void stopVisualization();

signals:
    void backRequested();

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onRenderTick();
    void onPlayPauseClicked();
    void onSeekChanged(int value);
    void onVolumeChanged(int value);
    void onVisModeChanged(int index);
    void onFftSizeChanged(int index);
    void onPositionChanged(qint64 ms);
    void onDurationChanged(qint64 ms);

private:
    AudioEngine* m_audioEngine;              ///< 音频引擎
    FFTAnalyzer m_fftAnalyzer;               ///< FFT 分析器（值成员，自动析构）
    QStackedWidget* m_visStack;              ///< 可视化组件堆栈
    BarSpectrumVisualizer* m_barWidget;       ///< 柱状频谱
    CircularSpectrumVisualizer* m_circularWidget;///< 圆形频谱
    WaveformVisualizer* m_waveformWidget;     ///< 波形
    WaterfallVisualizer* m_waterfallWidget;   ///< 瀑布图
    QPushButton* m_playPauseBtn;             ///< 播放/暂停按钮
    QPushButton* m_backBtn;                  ///< 返回按钮
    QSlider* m_positionSlider;               ///< 进度滑块
    QSlider* m_volumeSlider;                 ///< 音量滑块
    QSlider* m_compressionSlider;            ///< 频谱压缩强度滑块
    QLabel* m_compressionLabel;              ///< 压缩强度值显示
    QComboBox* m_fftSizeCombo;               ///< FFT 窗口大小选择
    QComboBox* m_visModeCombo;               ///< 可视化模式选择
    QLabel* m_timeLabel;                     ///< 时间显示标签
    QTimer* m_renderTimer;                   ///< 渲染定时器（60Hz）
    qint64 m_durationMs;                     ///< 音频总时长
    float m_compressionPower;                ///< 频谱压缩强度（0.05-1.0）
    bool m_seeking;                          ///< 是否正在拖动进度条
};
