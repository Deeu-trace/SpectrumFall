#pragma once

#include <QObject>
#include <QVector>

/// 节拍点数据结构
struct BeatPoint
{
    qint64 timestampMs;  ///< 节拍时间戳（毫秒）
    int lane;            ///< 分配轨道（0=D, 1=F, 2=J, 3=K）
};

/// 节拍检测器：基于 Spectral Flux + 自适应阈值
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    explicit BeatDetector(QObject* parent = nullptr);

    /// 分析 PCM 数据，检测节拍
    void analyze(const QVector<float>& pcm, int sampleRate);

    /// 获取检测到的 BPM
    float bpm() const;

    /// 获取所有节拍点（含轨道分配）
    const QVector<BeatPoint>& beatPoints() const;

private:
    float m_bpm;                    ///< 检测到的 BPM
    QVector<BeatPoint> m_beatPoints;///< 节拍点列表
    int m_fftSize;                  ///< FFT 窗口大小
    int m_hopSize;                  ///< 帧移大小

    /// 计算 Spectral Flux（频谱通量），同时缓存每帧幅度谱
    QVector<float> computeSpectralFlux(const QVector<float>& pcm, int sampleRate,
                                       QVector<QVector<float>>& magnitudeCache);

    /// 自适应阈值：局部均值 + δ * 局部标准差
    float adaptiveThreshold(const QVector<float>& flux, int index, int windowSize);

    /// 在 Spectral Flux 中寻找峰值
    QVector<int> findPeaks(const QVector<float>& flux, float threshold);

    /// 根据峰值间隔估计 BPM
    float estimateBPM(const QVector<int>& peakFrames, int hopSize, int sampleRate);

    /// 基于频谱能量分配轨道
    int assignLane(const QVector<float>& magnitude, int fftSize, int sampleRate);
};
