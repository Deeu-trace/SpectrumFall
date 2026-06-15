#pragma once

#include <QWidget>
#include <QVector>

/// 可视化组件基类：提供频谱和波形数据的设置接口
class VisualizerBase : public QWidget
{
    Q_OBJECT

public:
    explicit VisualizerBase(QWidget* parent = nullptr);

    /// 设置频谱数据（归一化幅度，长度为 FFT 窗口的一半）
    virtual void setSpectrumData(const QVector<float>& magnitude);

    /// 设置波形数据（时域 PCM 窗口）
    virtual void setWaveformData(const QVector<float>& samples);

protected:
    QVector<float> m_magnitude;  ///< 频谱幅度数据
    QVector<float> m_waveform;   ///< 波形数据
};
