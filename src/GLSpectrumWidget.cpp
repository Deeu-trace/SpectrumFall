#include "GLSpectrumWidget.h"
#include <QDateTime>
#include <QVector2D>
#include <cstring>

// ── Windows OpenGL 常量补丁 ──
// Windows SDK 的 <GL/gl.h> 仅提供 OpenGL 1.1 定义，
// 以下常量属于 1.2+，在未安装扩展头文件时需要手动定义。
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

// ═══════════════════════════════════════════════════════════════
//  共享顶点着色器：全屏四边形
// ═══════════════════════════════════════════════════════════════
static const char* kVertShader = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;
out vec2 v_uv;
void main() {
    v_uv = a_pos * 0.5 + 0.5;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

// ═══════════════════════════════════════════════════════════════
//  赛博朋克背景：稀疏星光 + 旋转六边形光环 + 低音径向波纹
// ═══════════════════════════════════════════════════════════════
static const char* kBgFragShader = R"(
#version 330 core
in vec2 v_uv;
out vec4 fragColor;

#define MAX_BARS 256
uniform float spectrum[MAX_BARS];
uniform int   barCount;
uniform float time;
uniform vec2  resolution;

const float TAU = 6.28318530718;

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    float aspect = resolution.x / max(resolution.y, 1.0);
    vec2 uv = v_uv;
    vec2 p = uv * 2.0 - 1.0;
    p.x *= aspect;

    float dist = length(p);
    vec3 color = vec3(0.015, 0.015, 0.04);

    // ── 低音能量 ──
    float bassLevel = 0.0;
    for (int i = 0; i < 8 && i < barCount; i++) {
        bassLevel += spectrum[i];
    }
    bassLevel = clamp(bassLevel / 8.0, 0.0, 1.0);

    // ── 径向渐变（中心微亮）──
    color += vec3(0.025, 0.02, 0.05) * smoothstep(1.5, 0.0, dist);

    // ── 星光粒子（稀疏 40 颗）──
    for (int i = 0; i < 40; i++) {
        float fi = float(i);
        float sx = (hash21(vec2(fi, 1.0)) - 0.5) * 2.0 * aspect;
        float sy = hash21(vec2(fi, 2.0)) * 2.0 - 1.0;
        vec2 starPos = vec2(sx, sy);

        float sd = length(p - starPos);
        float twinkle = 0.4 + 0.6 * sin(time * (0.8 + hash21(vec2(fi, 3.0)) * 1.5) + fi * 1.7);
        float star = smoothstep(0.012, 0.0, sd) * twinkle;

        // 星光十字芒
        float crossH = smoothstep(0.002, 0.0, abs(p.y - sy)) * smoothstep(0.04, 0.0, abs(p.x - sx));
        float crossV = smoothstep(0.002, 0.0, abs(p.x - sx)) * smoothstep(0.04, 0.0, abs(p.y - sy));
        star += (crossH + crossV) * twinkle * 0.3;

        float starHue = 0.52 + hash21(vec2(fi, 4.0)) * 0.2;
        vec3 starColor = hsv2rgb(vec3(starHue, 0.4, 1.0));
        color += starColor * star * 0.35;
    }

    // ── 低音径向波纹（从中心扩散）──
    if (bassLevel > 0.12) {
        for (int w = 0; w < 3; w++) {
            float waveTime = fract(time * 0.35 + float(w) * 0.333);
            float waveR = waveTime * 1.3;
            float waveWidth = 0.004 + bassLevel * 0.006;
            float wave = smoothstep(waveWidth, 0.0, abs(dist - waveR));
            float waveFade = (1.0 - waveTime) * smoothstep(0.0, 0.1, waveTime);
            color += vec3(0.0, 0.5, 0.7) * wave * waveFade * bassLevel * 0.35;
        }
    }

    // ── 暗角 ──
    float vig = smoothstep(1.3, 0.35, length(uv - 0.5));
    color *= 0.65 + 0.35 * vig;

    fragColor = vec4(color, 1.0);
}
)";

// ═══════════════════════════════════════════════════════════════
//  柱体着色器：HSL彩虹 + 峰值保持点 + 镜像倒影 + 低音呼吸
// ═══════════════════════════════════════════════════════════════
static const char* kBarFragShader = R"(
#version 330 core
in vec2 v_uv;
out vec4 fragColor;

#define MAX_BARS 256
uniform float spectrum[MAX_BARS];
uniform float peaks[MAX_BARS];
uniform int   barCount;
uniform float compressionPower;
uniform float time;

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec2 uv = v_uv;
    vec4 outColor = vec4(0.0);

    float baseline = 0.12;
    float usableHeight = 1.0 - baseline - 0.04;

    float bw = 1.0 / float(barCount);
    int bi = int(uv.x / bw);

    // 低音能量 → 呼吸亮度
    float bassLevel = 0.0;
    for (int i = 0; i < 8 && i < barCount; i++) {
        bassLevel += spectrum[i];
    }
    bassLevel = clamp(bassLevel / 8.0, 0.0, 1.0);
    float breathing = 0.7 + bassLevel * 0.6;

    if (bi >= 0 && bi < barCount && bi < MAX_BARS) {
        float val = pow(clamp(spectrum[bi], 0.0, 1.0), compressionPower);
        float barH = val * usableHeight;
        float barCenter = (float(bi) + 0.5) * bw;
        float barHalfW = bw * 0.40;

        float xDist = abs(uv.x - barCenter);
        float yFromBase = uv.y - baseline;

        // HSL 彩虹色，随时间缓慢漂移
        float hue = float(bi) / float(barCount) + time * 0.03;
        vec3 barColor = hsv2rgb(vec3(fract(hue), 0.85, 1.0));

        // 柱体内部：底部暗→顶部亮
        float t = clamp(yFromBase / max(barH, 0.001), 0.0, 1.0);
        vec3 innerColor = barColor * (0.4 + 0.6 * t);

        // 柱体填充
        float xFill = smoothstep(barHalfW + 0.003, barHalfW - 0.003, xDist);
        float yFill = step(0.0, yFromBase) * step(yFromBase, barH);
        float barMask = xFill * yFill;

        // 柱顶柔光
        float xGlow = max(0.0, xDist - barHalfW);
        float yGlow = max(0.0, yFromBase - barH);
        float glowDist = length(vec2(xGlow, yGlow));
        float glow = smoothstep(0.05, 0.0, glowDist) * val * 0.5;

        // 峰值保持点
        float peakVal = pow(clamp(peaks[bi], 0.0, 1.0), compressionPower);
        float peakY = peakVal * usableHeight;
        float peakYDist = abs(yFromBase - peakY);
        float peakDot = smoothstep(0.01, 0.0, peakYDist) * xFill;
        float peakGlow = smoothstep(0.03, 0.0, peakYDist) * xFill * 0.3;
        vec3 peakColor = hsv2rgb(vec3(fract(hue + 0.15), 0.5, 1.0));

        // 镜像倒影（基线以下）
        if (uv.y < baseline) {
            float reflectDist = baseline - uv.y;
            float reflectMaxH = barH * 0.35;
            if (reflectDist < reflectMaxH) {
                float reflectFade = 1.0 - reflectDist / max(reflectMaxH, 0.001);
                outColor.rgb += barColor * xFill * reflectFade * 0.12;
                outColor.a = max(outColor.a, xFill * reflectFade * 0.15);
            }
        }

        // 基线辉光带
        float baseGlow = smoothstep(0.015, 0.0, abs(yFromBase)) * 0.25 * breathing;

        outColor.rgb += (innerColor * barMask + peakColor * peakDot
                       + innerColor * peakGlow) * breathing
                      + barColor * glow * breathing
                      + barColor * baseGlow;
        outColor.a = max(outColor.a, barMask + peakDot + peakGlow
                       + glow * 0.5 + baseGlow);
    }

    fragColor = outColor;
}
)";

// ═══════════════════════════════════════════════════════════════
//  圆形频谱着色器：HSL彩虹弧 + 峰值点 + 中心脉冲 + 外环粒子
// ═══════════════════════════════════════════════════════════════
static const char* kCircleFragShader = R"(
#version 330 core
in vec2 v_uv;
out vec4 fragColor;

#define MAX_BARS 256
uniform float spectrum[MAX_BARS];
uniform float peaks[MAX_BARS];
uniform int   barCount;
uniform float time;
uniform float compressionPower;
uniform vec2  resolution;

const float TAU = 6.28318530718;

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    float aspect = resolution.x / max(resolution.y, 1.0);
    vec2 p = v_uv * 2.0 - 1.0;
    p.x *= aspect;

    float r = length(p);
    float angle = atan(p.y, p.x);
    float normAngle = angle / TAU + 0.5;
    float rotAngle = fract(normAngle + time * 0.02);

    vec4 outColor = vec4(0.0);

    float innerR    = 0.30;
    float maxBarLen = 0.55;

    // 低音能量 → 呼吸
    float bassLevel = 0.0;
    for (int i = 0; i < 8 && i < barCount; i++) {
        bassLevel += spectrum[i];
    }
    bassLevel = clamp(bassLevel / 8.0, 0.0, 1.0);
    float breathing = 0.7 + bassLevel * 0.6;

    // 中心脉冲
    float pulseR = innerR * (0.6 + bassLevel * 0.5);
    float pulse = smoothstep(pulseR, 0.0, r);
    vec3 pulseColor = hsv2rgb(vec3(0.55 + bassLevel * 0.1, 0.8, 1.0));
    outColor.rgb += pulseColor * pulse * 0.15 * breathing;
    outColor.a = max(outColor.a, pulse * 0.2);

    // 内环辉光
    float ringW = 0.005 + bassLevel * 0.003;
    float ring = smoothstep(ringW, 0.0, abs(r - innerR));
    vec3 ringColor = hsv2rgb(vec3(0.5, 0.7, 1.0));
    outColor.rgb += ringColor * ring * (0.4 + bassLevel * 0.4) * breathing;
    outColor.a = max(outColor.a, ring * 0.6);

    // 频谱弧线
    float bw = 1.0 / float(barCount);
    int bi = int(rotAngle / bw);

    if (bi >= 0 && bi < barCount && bi < MAX_BARS) {
        float val = pow(clamp(spectrum[bi], 0.0, 1.0), compressionPower);
        float barLen = val * maxBarLen;
        float outerR = innerR + barLen;

        float barCenter = (float(bi) + 0.5) * bw;
        float angDist = abs(rotAngle - barCenter);
        angDist = min(angDist, 1.0 - angDist);
        float barHalfW = bw * 0.35;
        float angFill = smoothstep(barHalfW + 0.003, barHalfW - 0.003, angDist);

        float radInner = smoothstep(innerR - 0.008, innerR + 0.003, r);
        float radOuter = smoothstep(outerR + 0.004, outerR - 0.002, r);
        float barMask = angFill * radInner * radOuter;

        // HSL 彩虹色
        float hue = float(bi) / float(barCount) + time * 0.03;
        vec3 barColor = hsv2rgb(vec3(fract(hue), 0.85, 1.0));

        // 弧线顶端柔光
        float tipGlow = smoothstep(0.04, 0.0, r - outerR)
                      * smoothstep(0.0, 0.04, r - innerR)
                      * angFill * val * 0.5;

        // 峰值保持点
        float peakVal = pow(clamp(peaks[bi], 0.0, 1.0), compressionPower);
        float peakR = innerR + peakVal * maxBarLen;
        float peakDot = smoothstep(0.012, 0.0, abs(r - peakR)) * angFill;
        float peakGlow = smoothstep(0.03, 0.0, abs(r - peakR)) * angFill * 0.3;
        vec3 peakColor = hsv2rgb(vec3(fract(hue + 0.15), 0.5, 1.0));

        outColor.rgb += (barColor * barMask + peakColor * peakDot
                       + barColor * peakGlow + barColor * tipGlow) * breathing;
        outColor.a = max(outColor.a, barMask + peakDot + peakGlow + tipGlow * 0.5);
    }

    // 外环旋转粒子（24 颗）
    float outerRingR = innerR + maxBarLen + 0.08;
    for (int i = 0; i < 24; i++) {
        float fi = float(i);
        float particleAngle = fract(fi / 24.0 + time * 0.05) * TAU - 3.14159;
        float angDiff = abs(angle - particleAngle);
        angDiff = min(angDiff, TAU - angDiff);

        float particleR = outerRingR + sin(time * 2.0 + fi) * 0.02;
        float distToParticle = length(vec2(angDiff * particleR, r - particleR));
        float particle = smoothstep(0.025, 0.0, distToParticle);

        float pHue = fi / 24.0 + time * 0.05;
        vec3 pColor = hsv2rgb(vec3(fract(pHue), 0.9, 1.0));
        outColor.rgb += pColor * particle * (0.3 + bassLevel * 0.3) * breathing;
        outColor.a = max(outColor.a, particle * 0.5);
    }

    fragColor = outColor;
}
)";

// ═══════════════════════════════════════════════════════════════
//  高斯模糊着色器：9-tap，方向由 uniform 控制
// ═══════════════════════════════════════════════════════════════
static const char* kBlurFragShader = R"(
#version 330 core
in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D tex;
uniform vec2 texelSize;
uniform int direction; // 0=horizontal, 1=vertical

void main() {
    vec2 dir = (direction == 0) ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);

    vec4 sum = texture(tex, v_uv) * 0.227027;
    sum += texture(tex, v_uv + dir * 1.0) * 0.1945946;
    sum += texture(tex, v_uv - dir * 1.0) * 0.1945946;
    sum += texture(tex, v_uv + dir * 2.0) * 0.1216216;
    sum += texture(tex, v_uv - dir * 2.0) * 0.1216216;
    sum += texture(tex, v_uv + dir * 3.0) * 0.054054;
    sum += texture(tex, v_uv - dir * 3.0) * 0.054054;
    sum += texture(tex, v_uv + dir * 4.0) * 0.016216;
    sum += texture(tex, v_uv - dir * 4.0) * 0.016216;

    fragColor = sum;
}
)";

// ═══════════════════════════════════════════════════════════════
//  纹理直通着色器：合成 pass 用
// ═══════════════════════════════════════════════════════════════
static const char* kTexFragShader = R"(
#version 330 core
in vec2 v_uv;
out vec4 fragColor;
uniform sampler2D tex;
void main() {
    fragColor = texture(tex, v_uv);
}
)";

// ═══════════════════════════════════════════════════════════════
//  C++ 实现
// ═══════════════════════════════════════════════════════════════

GLSpectrumWidget::GLSpectrumWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_bgShader(nullptr)
    , m_barShader(nullptr)
    , m_circleShader(nullptr)
    , m_blurShader(nullptr)
    , m_texShader(nullptr)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
    , m_sceneFbo(nullptr)
    , m_blurFbo{nullptr, nullptr}
    , m_fboW(0)
    , m_fboH(0)
    , m_dataCount(0)
    , m_time(0.0f)
    , m_compressionPower(0.33f)
    , m_lastFrameMs(0)
    , m_renderMode(Bars)
{
    memset(m_spectrum, 0, sizeof(m_spectrum));
    memset(m_peaks, 0, sizeof(m_peaks));
    memset(m_peakVel, 0, sizeof(m_peakVel));
}

GLSpectrumWidget::~GLSpectrumWidget()
{
    makeCurrent();
    delete m_bgShader;    m_bgShader = nullptr;
    delete m_barShader;   m_barShader = nullptr;
    delete m_circleShader; m_circleShader = nullptr;
    delete m_blurShader;  m_blurShader = nullptr;
    delete m_texShader;   m_texShader = nullptr;
    delete m_sceneFbo;   m_sceneFbo = nullptr;
    delete m_blurFbo[0]; m_blurFbo[0] = nullptr;
    delete m_blurFbo[1]; m_blurFbo[1] = nullptr;
    m_vbo.destroy();
    m_vao.destroy();
    doneCurrent();
}

bool GLSpectrumWidget::initShaders()
{
    // 背景着色器
    m_bgShader = new QOpenGLShaderProgram(this);
    if (!m_bgShader->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertShader) ||
        !m_bgShader->addShaderFromSourceCode(QOpenGLShader::Fragment, kBgFragShader) ||
        !m_bgShader->link()) {
        qWarning() << "GLSpectrum: bg shader failed:" << m_bgShader->log();
        return false;
    }

    // 柱体着色器
    m_barShader = new QOpenGLShaderProgram(this);
    if (!m_barShader->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertShader) ||
        !m_barShader->addShaderFromSourceCode(QOpenGLShader::Fragment, kBarFragShader) ||
        !m_barShader->link()) {
        qWarning() << "GLSpectrum: bar shader failed:" << m_barShader->log();
        return false;
    }

    // 圆形频谱着色器
    m_circleShader = new QOpenGLShaderProgram(this);
    if (!m_circleShader->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertShader) ||
        !m_circleShader->addShaderFromSourceCode(QOpenGLShader::Fragment, kCircleFragShader) ||
        !m_circleShader->link()) {
        qWarning() << "GLSpectrum: circle shader failed:" << m_circleShader->log();
        return false;
    }

    // 模糊着色器
    m_blurShader = new QOpenGLShaderProgram(this);
    if (!m_blurShader->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertShader) ||
        !m_blurShader->addShaderFromSourceCode(QOpenGLShader::Fragment, kBlurFragShader) ||
        !m_blurShader->link()) {
        qWarning() << "GLSpectrum: blur shader failed:" << m_blurShader->log();
        return false;
    }

    // 纹理直通着色器
    m_texShader = new QOpenGLShaderProgram(this);
    if (!m_texShader->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertShader) ||
        !m_texShader->addShaderFromSourceCode(QOpenGLShader::Fragment, kTexFragShader) ||
        !m_texShader->link()) {
        qWarning() << "GLSpectrum: tex shader failed:" << m_texShader->log();
        return false;
    }

    return true;
}

void GLSpectrumWidget::initQuad()
{
    static const float kQuadVerts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
    };

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(kQuadVerts, sizeof(kQuadVerts));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    m_vbo.release();
    m_vao.release();
}

void GLSpectrumWidget::ensureFBOs(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (w == m_fboW && h == m_fboH && m_sceneFbo) return;

    delete m_sceneFbo;   m_sceneFbo = nullptr;
    delete m_blurFbo[0]; m_blurFbo[0] = nullptr;
    delete m_blurFbo[1]; m_blurFbo[1] = nullptr;

    QOpenGLFramebufferObjectFormat fmt;
    fmt.setInternalTextureFormat(GL_RGBA8);
    fmt.setAttachment(QOpenGLFramebufferObject::NoAttachment);

    m_sceneFbo   = new QOpenGLFramebufferObject(w, h, fmt);
    m_blurFbo[0] = new QOpenGLFramebufferObject(w, h, fmt);
    m_blurFbo[1] = new QOpenGLFramebufferObject(w, h, fmt);

    // 设置纹理过滤为线性（模糊需要平滑插值）
    for (auto* fbo : {m_sceneFbo, m_blurFbo[0], m_blurFbo[1]}) {
        glBindTexture(GL_TEXTURE_2D, fbo->texture());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    m_fboW = w;
    m_fboH = h;
}

void GLSpectrumWidget::setSpectrumUniform(QOpenGLShaderProgram* shader)
{
    int loc = shader->uniformLocation("spectrum");
    if (loc >= 0 && m_dataCount > 0) {
        glUniform1fv(loc, 256, m_spectrum);
    }
    int peakLoc = shader->uniformLocation("peaks");
    if (peakLoc >= 0) {
        glUniform1fv(peakLoc, 256, m_peaks);
    }
    shader->setUniformValue("barCount", m_dataCount > 0 ? m_dataCount : 256);
    shader->setUniformValue("time", m_time);
    shader->setUniformValue("compressionPower", m_compressionPower);
    shader->setUniformValue("resolution", QVector2D(width(), height()));
}

void GLSpectrumWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.02f, 0.02f, 0.06f, 1.0f);

    if (!initShaders()) {
        qWarning() << "GLSpectrumWidget: shader init failed";
        return;
    }
    initQuad();
}

void GLSpectrumWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GLSpectrumWidget::paintGL()
{
    // 更新时间
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_lastFrameMs > 0) {
        m_time += static_cast<float>(nowMs - m_lastFrameMs) / 1000.0f;
    }
    m_lastFrameMs = nowMs;

    QOpenGLShaderProgram* sceneShader =
        (m_renderMode == Circular) ? m_circleShader : m_barShader;

    if (!sceneShader || !m_vao.isCreated() || !m_bgShader) {
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    m_vao.bind();

    // ── 背景层（不透明，赛博朋克星光+六边形+波纹）──
    glDisable(GL_BLEND);
    m_bgShader->bind();
    setSpectrumUniform(m_bgShader);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    m_bgShader->release();

    // ── 场景层（加法混合，霓虹柱/弧线叠在背景上）──
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    sceneShader->bind();
    setSpectrumUniform(sceneShader);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    sceneShader->release();

    glDisable(GL_BLEND);
    m_vao.release();
}

void GLSpectrumWidget::setSpectrumData(const QVector<float>& magnitude)
{
    int count = qMin(magnitude.size(), 256);

    // 帧间平滑 + 峰值保持
    const float smoothFactor = 0.35f;  // 越大越灵敏，越小越丝滑
    for (int i = 0; i < count; ++i) {
        // lerp 平滑
        m_spectrum[i] = m_spectrum[i] + (magnitude[i] - m_spectrum[i]) * smoothFactor;

        // 峰值：重力下落
        if (m_spectrum[i] > m_peaks[i]) {
            m_peaks[i] = m_spectrum[i];
            m_peakVel[i] = 0.0f;
        } else {
            m_peakVel[i] -= 0.003f;  // 重力加速度
            m_peaks[i] += m_peakVel[i];
            if (m_peaks[i] < 0.0f) {
                m_peaks[i] = 0.0f;
                m_peakVel[i] = 0.0f;
            }
        }
    }
    for (int i = count; i < 256; ++i) {
        m_spectrum[i] = 0.0f;
        m_peaks[i] = 0.0f;
        m_peakVel[i] = 0.0f;
    }
    m_dataCount = count;
    update();
}

void GLSpectrumWidget::setCompressionPower(float power)
{
    m_compressionPower = qBound(0.05f, power, 1.0f);
    update();
}

void GLSpectrumWidget::setRenderMode(RenderMode mode)
{
    m_renderMode = mode;
    update();
}
