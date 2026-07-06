#include "UIButtonEffects.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QPushButton>
#include <QSoundEffect>
#include <QEvent>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>

// ── 内部实现 ──────────────────────────────────────────────────────
class ButtonEffectFilter : public QObject
{
public:
    explicit ButtonEffectFilter(QObject* parent = nullptr);

    void install(QApplication* app);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void playClickSound();
    void applyGlow(QPushButton* btn);
    void removeGlow(QPushButton* btn);

    QSoundEffect* m_clickSound;
};

// ── 单例 ──────────────────────────────────────────────────────────
static ButtonEffectFilter* s_instance = nullptr;

ButtonEffectFilter::ButtonEffectFilter(QObject* parent)
    : QObject(parent)
    , m_clickSound(new QSoundEffect(this))
{
    m_clickSound->setSource(QUrl(QStringLiteral("qrc:/game/click.wav")));
    m_clickSound->setVolume(0.5f);
}

void ButtonEffectFilter::install(QApplication* app)
{
    app->installEventFilter(this);
}

// ── 音效 ──────────────────────────────────────────────────────────
void ButtonEffectFilter::playClickSound()
{
    if (m_clickSound->isLoaded())
        m_clickSound->play();
}

// ── 辉光效果（直接设置，无动画） ──────────────────────────────────
static QColor glowColorForButton(QPushButton* btn)
{
    ThemePalette p = ThemeManager::instance()->menuPalette();
    QString name = btn->objectName();
    if (name == QStringLiteral("backButton"))
        return QColor(p.glowSecondary);
    return QColor(p.glowPrimary);
}

void ButtonEffectFilter::applyGlow(QPushButton* btn)
{
    // 已有辉光则跳过
    if (btn->graphicsEffect()) return;

    auto* glow = new QGraphicsDropShadowEffect(btn);
    glow->setBlurRadius(28);
    glow->setColor(glowColorForButton(btn));
    glow->setOffset(0, 0);
    btn->setGraphicsEffect(glow);
}

void ButtonEffectFilter::removeGlow(QPushButton* btn)
{
    btn->setGraphicsEffect(nullptr);
}

// ── 事件过滤 ──────────────────────────────────────────────────────
bool ButtonEffectFilter::eventFilter(QObject* watched, QEvent* event)
{
    auto* btn = qobject_cast<QPushButton*>(watched);
    if (!btn || !btn->isEnabled())
        return false;

    switch (event->type()) {
    case QEvent::HoverEnter:
        applyGlow(btn);
        break;

    case QEvent::HoverLeave:
        removeGlow(btn);
        break;

    case QEvent::MouseButtonPress:
        if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
            playClickSound();
        break;

    default:
        break;
    }

    return false;
}

// ── 公共接口 ──────────────────────────────────────────────────────
void UIButtonEffects::install(QApplication* app)
{
    if (!s_instance) {
        s_instance = new ButtonEffectFilter(app);
    }
    s_instance->install(app);
}
