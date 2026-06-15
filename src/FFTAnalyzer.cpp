#define _USE_MATH_DEFINES
#include "FFTAnalyzer.h"
#include <algorithm>
#include <cmath>

FFTAnalyzer::FFTAnalyzer(int windowSize)
    : m_windowSize(windowSize)
    , m_log2N(0)
{
    setWindowSize(windowSize);
}

void FFTAnalyzer::setWindowSize(int size)
{
    // 确保 size 是 2 的幂
    int n = 1;
    m_log2N = 0;
    while (n < size) {
        n <<= 1;
        ++m_log2N;
    }
    m_windowSize = n;
    buildTwiddleFactors();
    buildHannWindow();
}

int FFTAnalyzer::windowSize() const
{
    return m_windowSize;
}

void FFTAnalyzer::buildTwiddleFactors()
{
    int N = m_windowSize;
    m_twiddleCos.resize(N / 2);
    m_twiddleSin.resize(N / 2);
    for (int i = 0; i < N / 2; ++i) {
        double angle = -2.0 * M_PI * i / N;
        m_twiddleCos[i] = static_cast<float>(std::cos(angle));
        m_twiddleSin[i] = static_cast<float>(std::sin(angle));
    }
}

void FFTAnalyzer::buildHannWindow()
{
    int N = m_windowSize;
    m_hannWindow.resize(N);
    for (int i = 0; i < N; ++i) {
        m_hannWindow[i] = static_cast<float>(0.5 * (1.0 - std::cos(2.0 * M_PI * i / (N - 1))));
    }
}

int FFTAnalyzer::bitReverse(int x, int log2n)
{
    int result = 0;
    for (int i = 0; i < log2n; ++i) {
        result <<= 1;
        result |= (x & 1);
        x >>= 1;
    }
    return result;
}

void FFTAnalyzer::compute(const QVector<float>& input, QVector<float>& magnitude)
{
    int N = m_windowSize;

    // 准备实部和虚部数组，应用 Hanning 窗和位逆序重排
    QVector<float> real(N, 0.0f);
    QVector<float> imag(N, 0.0f);

    int inputLen = qMin(input.size(), N);
    for (int i = 0; i < inputLen; ++i) {
        int j = bitReverse(i, m_log2N);
        real[j] = input[i] * m_hannWindow[i];
        imag[j] = 0.0f;
    }

    // 迭代式 Cooley-Tukey 蝶形运算
    for (int s = 1; s <= m_log2N; ++s) {
        int m = 1 << s;        // 当前蝶形运算的跨度
        int halfM = m >> 1;    // 跨度的一半
        int step = N / m;      // 旋转因子步进

        for (int k = 0; k < N; k += m) {
            for (int j = 0; j < halfM; ++j) {
                int twiddleIdx = j * step;
                float wr = m_twiddleCos[twiddleIdx];
                float wi = m_twiddleSin[twiddleIdx];

                int idxEven = k + j;
                int idxOdd = k + j + halfM;

                // 蝶形运算
                float tr = wr * real[idxOdd] - wi * imag[idxOdd];
                float ti = wr * imag[idxOdd] + wi * real[idxOdd];

                real[idxOdd] = real[idxEven] - tr;
                imag[idxOdd] = imag[idxEven] - ti;
                real[idxEven] += tr;
                imag[idxEven] += ti;
            }
        }
    }

    // 计算幅度谱（仅前 N/2 个正频率分量），归一化到 [0, 1]
    int halfN = N / 2;
    magnitude.resize(halfN);
    float maxMag = 0.0f;

    for (int i = 0; i < halfN; ++i) {
        magnitude[i] = std::sqrt(real[i] * real[i] + imag[i] * imag[i]);
        if (magnitude[i] > maxMag) {
            maxMag = magnitude[i];
        }
    }

    // 归一化
    if (maxMag > 1e-6f) {
        float invMax = 1.0f / maxMag;
        for (int i = 0; i < halfN; ++i) {
            magnitude[i] *= invMax;
        }
    }
}
