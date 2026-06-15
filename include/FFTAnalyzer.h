#pragma once

#include <QVector>
#include <cmath>

/// FFT 分析器：迭代式 Cooley-Tukey 算法
/// 输入：时域 PCM 数据（长度为 windowSize）
/// 输出：幅度谱（长度为 windowSize/2，归一化到 [0, 1]）
class FFTAnalyzer
{
public:
    explicit FFTAnalyzer(int windowSize = 2048);

    /// 设置窗口大小（必须是 2 的幂）
    void setWindowSize(int size);

    /// 获取当前窗口大小
    int windowSize() const;

    /// 执行 FFT：input 为时域数据，magnitude 为输出幅度谱
    void compute(const QVector<float>& input, QVector<float>& magnitude);

private:
    int m_windowSize;           ///< FFT 窗口大小
    int m_log2N;                ///< log2(windowSize)
    QVector<float> m_hannWindow;///< Hanning 窗函数系数
    QVector<float> m_twiddleCos;///< 旋转因子实部（预计算）
    QVector<float> m_twiddleSin;///< 旋转因子虚部（预计算）

    /// 预计算旋转因子表
    void buildTwiddleFactors();

    /// 构建 Hanning 窗
    void buildHannWindow();

    /// 位逆序计算
    static int bitReverse(int x, int log2n);
};
