#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFramebufferObject>
#include <QVector>

/// GLSL 着色器驱动的频谱可视化：粒子背景 + 霓虹柱状图/圆形频谱 + 真·辉光(Bloom)
/// 多 pass 渲染：场景→FBO，高斯模糊→Bloom，合成到屏幕
class GLSpectrumWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    enum RenderMode { Bars, Circular };

    explicit GLSpectrumWidget(QWidget* parent = nullptr);
    ~GLSpectrumWidget();

    void setSpectrumData(const QVector<float>& magnitude);
    void setCompressionPower(float power);
    void setRenderMode(RenderMode mode);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    // ── 着色器 ──
    QOpenGLShaderProgram* m_bgShader;       ///< 粒子背景（低音反应+冲击波）
    QOpenGLShaderProgram* m_barShader;      ///< 霓虹柱状图（仅柱体，透明背景）
    QOpenGLShaderProgram* m_circleShader;   ///< 霓虹圆形频谱（弧线+内环+中心脉冲）
    QOpenGLShaderProgram* m_blurShader;     ///< 高斯模糊（9-tap，方向可切换）
    QOpenGLShaderProgram* m_texShader;      ///< 纹理直通（用于合成 pass）

    // ── 几何 ──
    QOpenGLBuffer m_vbo;
    QOpenGLVertexArrayObject m_vao;

    // ── FBO（多 pass bloom）──
    QOpenGLFramebufferObject* m_sceneFbo;   ///< Pass1: 场景渲染目标
    QOpenGLFramebufferObject* m_blurFbo[2]; ///< Pass2-3: 模糊 ping-pong
    int m_fboW;
    int m_fboH;

    // ── 数据 ──
    float m_spectrum[256];       ///< 当前频谱（已平滑）
    float m_peaks[256];          ///< 峰值保持
    float m_peakVel[256];        ///< 峰值下落速度（重力）
    int m_dataCount;
    float m_time;
    float m_compressionPower;
    qint64 m_lastFrameMs;
    RenderMode m_renderMode;

    bool initShaders();
    void initQuad();
    void ensureFBOs(int w, int h);
    void setSpectrumUniform(QOpenGLShaderProgram* shader);
};
