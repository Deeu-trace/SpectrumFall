#include "AudioEffectWidget.h"
#include "AudioEffectProcessor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QFrame>

// ─────────────────────────────────────────────
//  样式
// ─────────────────────────────────────────────

static const char* kPanelQss =
    "AudioEffectWidget { background: transparent; border: none; }"
    "QLabel { background: transparent; color: #c8c8d8; }"
    "QSlider::groove:horizontal {"
    "  height: 4px; background: #1a1d2e; border-radius: 2px;"
    "}"
    "QSlider::handle:horizontal {"
    "  background: #00e090; width: 12px; height: 12px;"
    "  margin: -5px 0; border-radius: 6px;"
    "}"
    "QSlider::handle:horizontal:hover { background: #00ff9a; }"
    "QSlider::sub-page:horizontal { background: #00a86b; border-radius: 2px; }";

static const char* kTabQss =
    "QPushButton {"
    "  color: #8888aa; font-size: 12px; font-weight: bold;"
    "  background: #161827; border: none; border-radius: 4px;"
    "  padding: 6px 0;"
    "}"
    "QPushButton:hover { color: #ccccdd; background: #1e2138; }"
    "QPushButton:checked {"
    "  color: #00e090; background: #0a2a1e;"
    "  border: 1px solid #00a86b;"
    "}";

static const char* kSmallBtnQss =
    "QPushButton {"
    "  color: #ff6680; font-size: 11px;"
    "  background: transparent; border: 1px solid #ff6680; border-radius: 3px;"
    "  padding: 4px 16px;"
    "}"
    "QPushButton:hover { background: #ff6680; color: #fff; }";

static const char* kFilterTypeBtnQss =
    "QPushButton {"
    "  color: #44ccff; font-size: 11px; font-weight: bold;"
    "  background: transparent; border: 1px solid #44ccff; border-radius: 3px;"
    "  padding: 3px 10px;"
    "}"
    "QPushButton:hover { background: #44ccff; color: #000; }";

// ─────────────────────────────────────────────
//  辅助：创建一行 [标签 ─ 滑块 ─ 数值]
// ─────────────────────────────────────────────

static QWidget* makeSliderRow(const QString& label, int min, int max, int val,
                              const QString& suffix,
                              QSlider*& outSlider, QLabel*& outVal,
                              QWidget* parent)
{
    auto* w = new QWidget(parent);
    w->setStyleSheet("background: transparent;");
    auto* row = new QHBoxLayout(w);
    row->setContentsMargins(0, 2, 0, 2);
    row->setSpacing(8);

    auto* lb = new QLabel(label, w);
    lb->setFixedWidth(36);
    lb->setStyleSheet("color: #8888aa; font-size: 11px; background: transparent;");
    row->addWidget(lb);

    outSlider = new QSlider(Qt::Horizontal, w);
    outSlider->setRange(min, max);
    outSlider->setValue(val);
    row->addWidget(outSlider, 1);

    outVal = new QLabel(w);
    outVal->setFixedWidth(52);
    outVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    outVal->setStyleSheet("color: #00e090; font-size: 11px; font-weight: bold; background: transparent;");
    outVal->setText(QStringLiteral("%1%2").arg(val).arg(suffix));
    row->addWidget(outVal);

    return w;
}

// ─────────────────────────────────────────────
//  构造
// ─────────────────────────────────────────────

AudioEffectWidget::AudioEffectWidget(AudioEffectProcessor* processor, QWidget* parent)
    : QWidget(parent)
    , m_processor(processor)
    , m_currentTab(0)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(kPanelQss);
    setFixedSize(280, 290);
    buildUI();
    selectTab(0);
}

bool AudioEffectWidget::hasActiveEffect() const
{
    return m_currentTab != 0;
}

// ─────────────────────────────────────────────
//  UI 构建
// ─────────────────────────────────────────────

void AudioEffectWidget::buildUI()
{
    // 主布局：只放一个半透明背景板
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* bgPanel = new QFrame(this);
    bgPanel->setObjectName("effectBgPanel");
    bgPanel->setStyleSheet(
        "#effectBgPanel {"
        "  background: rgba(40, 44, 70, 220);"
        "  border: 1px solid rgba(0, 224, 144, 0.3);"
        "  border-radius: 8px;"
        "}");
    mainLayout->addWidget(bgPanel);

    // 内容全部放在背景板里
    auto* root = new QVBoxLayout(bgPanel);
    root->setSpacing(8);
    root->setContentsMargins(12, 10, 12, 10);

    // ── 标题栏 ──
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(4);

        auto* title = new QLabel(QStringLiteral("音频特效"), this);
        title->setStyleSheet("color: #00e090; font-size: 14px; font-weight: bold; background: transparent;");
        row->addWidget(title);
        row->addStretch();

        auto* closeBtn = new QPushButton("x", this);
        closeBtn->setFixedSize(20, 20);
        closeBtn->setStyleSheet(
            "QPushButton { color: #666; background: transparent; border: none; font-size: 14px; }"
            "QPushButton:hover { color: #ff6680; }");
        connect(closeBtn, &QPushButton::clicked, this, &AudioEffectWidget::requestClose);
        row->addWidget(closeBtn);

        root->addLayout(row);
    }

    // ── Tab 按钮 ──
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(4);

        m_tabGroup = new QButtonGroup(this);
        m_tabGroup->setExclusive(true);

        auto* btn0 = new QPushButton(QStringLiteral("直通"), this);
        auto* btn1 = new QPushButton(QStringLiteral("回声"), this);
        auto* btn2 = new QPushButton(QStringLiteral("滤波"), this);
        auto* btn3 = new QPushButton(QStringLiteral("变调"), this);

        for (auto* b : { btn0, btn1, btn2, btn3 }) {
            b->setCheckable(true);
            b->setStyleSheet(kTabQss);
            row->addWidget(b);
            m_tabGroup->addButton(b);
        }

        btn0->setChecked(true);
        m_tabGroup->setId(btn0, 0);
        m_tabGroup->setId(btn1, 1);
        m_tabGroup->setId(btn2, 2);
        m_tabGroup->setId(btn3, 3);

        root->addLayout(row);
    }

    // ── 分隔线 ──
    {
        auto* sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        sep->setStyleSheet("background: #1a1d2e; border: none;");
        root->addWidget(sep);
    }

    // ── 参数堆栈 ──
    m_paramStack = new QStackedWidget(this);
    m_paramStack->setStyleSheet("background: transparent;");

    // Page 0: 直通
    {
        auto* page = new QWidget();
        auto* l = new QVBoxLayout(page);
        l->setAlignment(Qt::AlignCenter);
        auto* hint = new QLabel(QStringLiteral("音频原样输出\n选择效果开始调节"), page);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color: #555; font-size: 11px; background: transparent;");
        l->addWidget(hint);
        m_paramStack->addWidget(page);
    }

    // Page 1: 回声
    {
        auto* page = new QWidget();
        auto* l = new QVBoxLayout(page);
        l->setSpacing(6);
        l->setContentsMargins(4, 6, 4, 6);

        l->addWidget(makeSliderRow(QStringLiteral("延迟"), 20, 2000, 300, QStringLiteral("ms"),
                     m_echoDelaySlider, m_echoDelayVal, page));
        l->addWidget(makeSliderRow(QStringLiteral("反馈"), 0, 95, 40, QStringLiteral("%"),
                     m_echoFeedbackSlider, m_echoFeedbackVal, page));
        l->addWidget(makeSliderRow(QStringLiteral("混合"), 0, 100, 50, QStringLiteral("%"),
                     m_echoMixSlider, m_echoMixVal, page));
        l->addStretch();

        connect(m_echoDelaySlider, &QSlider::valueChanged, this, [this](int v) {
            m_echoDelayVal->setText(QStringLiteral("%1ms").arg(v));
            m_processor->setEchoDelayMs(static_cast<float>(v));
            emit effectChanged();
        });
        connect(m_echoFeedbackSlider, &QSlider::valueChanged, this, [this](int v) {
            m_echoFeedbackVal->setText(QStringLiteral("%1%").arg(v));
            m_processor->setEchoFeedback(v / 100.0f);
            emit effectChanged();
        });
        connect(m_echoMixSlider, &QSlider::valueChanged, this, [this](int v) {
            m_echoMixVal->setText(QStringLiteral("%1%").arg(v));
            m_processor->setEchoMix(v / 100.0f);
            emit effectChanged();
        });

        m_paramStack->addWidget(page);
    }

    // Page 2: 滤波
    {
        auto* page = new QWidget();
        auto* l = new QVBoxLayout(page);
        l->setSpacing(6);
        l->setContentsMargins(4, 6, 4, 6);

        // 类型切换行
        {
            auto* row = new QHBoxLayout();
            row->setSpacing(8);

            m_filterTypeBtn = new QPushButton(QStringLiteral("低通"), page);
            m_filterTypeBtn->setStyleSheet(kFilterTypeBtnQss);
            row->addWidget(m_filterTypeBtn);

            auto* typeLabel = new QLabel(QStringLiteral("LowPass"), page);
            typeLabel->setStyleSheet("color: #888; font-size: 10px; background: transparent;");
            row->addWidget(typeLabel);
            row->addStretch();

            connect(m_filterTypeBtn, &QPushButton::clicked, this, [this, typeLabel]() {
                m_filterIsLowPass = !m_filterIsLowPass;
                if (m_filterIsLowPass) {
                    m_filterTypeBtn->setText(QStringLiteral("低通"));
                    typeLabel->setText(QStringLiteral("LowPass"));
                    if (m_currentTab == 2)
                        m_processor->setEffectType(AudioEffectType::LowPass);
                } else {
                    m_filterTypeBtn->setText(QStringLiteral("高通"));
                    typeLabel->setText(QStringLiteral("HighPass"));
                    if (m_currentTab == 2)
                        m_processor->setEffectType(AudioEffectType::HighPass);
                }
                emit effectChanged();
            });

            l->addLayout(row);
        }

        l->addWidget(makeSliderRow(QStringLiteral("频率"), 50, 15000, 1000, QStringLiteral("Hz"),
                     m_filterCutoffSlider, m_filterCutoffVal, page));
        l->addWidget(makeSliderRow(QStringLiteral("共振"), 0, 95, 0, QStringLiteral("%"),
                     m_filterResonanceSlider, m_filterResonanceVal, page));
        l->addStretch();

        connect(m_filterCutoffSlider, &QSlider::valueChanged, this, [this](int v) {
            m_filterCutoffVal->setText(QStringLiteral("%1Hz").arg(v));
            m_processor->setFilterCutoffHz(static_cast<float>(v));
            emit effectChanged();
        });
        connect(m_filterResonanceSlider, &QSlider::valueChanged, this, [this](int v) {
            m_filterResonanceVal->setText(QStringLiteral("%1%").arg(v));
            m_processor->setFilterResonance(v / 100.0f);
            emit effectChanged();
        });

        m_paramStack->addWidget(page);
    }

    // Page 3: 变调
    {
        auto* page = new QWidget();
        auto* l = new QVBoxLayout(page);
        l->setSpacing(8);
        l->setContentsMargins(4, 10, 4, 6);

        l->addWidget(makeSliderRow(QStringLiteral("半音"), -12, 12, 0, QStringLiteral(""),
                     m_pitchSlider, m_pitchVal, page));

        auto* hint = new QLabel(QStringLiteral("范围: -12 ~ +12 半音"), page);
        hint->setStyleSheet("color: #555; font-size: 10px; background: transparent;");
        hint->setAlignment(Qt::AlignCenter);
        l->addWidget(hint);
        l->addStretch();

        connect(m_pitchSlider, &QSlider::valueChanged, this, [this](int v) {
            m_pitchVal->setText(QString::number(v));
            m_processor->setPitchSemitones(v);
            emit effectChanged();
        });

        m_paramStack->addWidget(page);
    }

    root->addWidget(m_paramStack, 1);

    // ── 底部重置 ──
    {
        auto* row = new QHBoxLayout();
        row->addStretch();
        auto* resetBtn = new QPushButton(QStringLiteral("重置参数"), this);
        resetBtn->setStyleSheet(kSmallBtnQss);
        connect(resetBtn, &QPushButton::clicked, this, &AudioEffectWidget::onResetClicked);
        row->addWidget(resetBtn);
        row->addStretch();
        root->addLayout(row);
    }

    // Tab 切换信号
    connect(m_tabGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &AudioEffectWidget::onTabChanged);
}

// ─────────────────────────────────────────────
//  Tab 切换
// ─────────────────────────────────────────────

void AudioEffectWidget::selectTab(int index)
{
    m_currentTab = index;
    m_paramStack->setCurrentIndex(index);

    switch (index) {
    case 0:
        m_processor->setEffectType(AudioEffectType::None);
        break;
    case 1:
        m_processor->setEffectType(AudioEffectType::Echo);
        syncAllParams();
        break;
    case 2:
        m_processor->setEffectType(m_filterIsLowPass ? AudioEffectType::LowPass
                                                      : AudioEffectType::HighPass);
        syncAllParams();
        break;
    case 3:
        m_processor->setEffectType(AudioEffectType::PitchShift);
        m_processor->setPitchSemitones(m_pitchSlider->value());
        break;
    }
    emit effectChanged();
}

void AudioEffectWidget::onTabChanged(int index)
{
    selectTab(index);
}

// ─────────────────────────────────────────────
//  重置
// ─────────────────────────────────────────────

void AudioEffectWidget::onResetClicked()
{
    m_echoDelaySlider->setValue(300);
    m_echoFeedbackSlider->setValue(40);
    m_echoMixSlider->setValue(50);
    m_filterCutoffSlider->setValue(1000);
    m_filterResonanceSlider->setValue(0);
    m_pitchSlider->setValue(0);

    auto* btn = m_tabGroup->button(0);
    if (btn) btn->setChecked(true);
    selectTab(0);
}

// ─────────────────────────────────────────────
//  同步所有参数到 processor
// ─────────────────────────────────────────────

void AudioEffectWidget::syncAllParams()
{
    if (!m_processor) return;
    m_processor->setEchoDelayMs(static_cast<float>(m_echoDelaySlider->value()));
    m_processor->setEchoFeedback(m_echoFeedbackSlider->value() / 100.0f);
    m_processor->setEchoMix(m_echoMixSlider->value() / 100.0f);
    m_processor->setFilterCutoffHz(static_cast<float>(m_filterCutoffSlider->value()));
    m_processor->setFilterResonance(m_filterResonanceSlider->value() / 100.0f);
    m_processor->setPitchSemitones(m_pitchSlider->value());
}
