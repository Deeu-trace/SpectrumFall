#include "ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QDebug>
#include <QSettings>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance()
{
    if (!s_instance) s_instance = new ThemeManager();
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent), m_currentMood(Mood::Default), m_autoMoodEnabled(true)
{
    QFile f(QStringLiteral(":/game/resources/style.qss"));
    if (f.open(QIODevice::ReadOnly))
        m_baseQss = QString::fromUtf8(f.readAll());
    else
        qWarning() << "ThemeManager: cannot load style.qss";

    QSettings settings(QStringLiteral("SpectrumFall"), QStringLiteral("SpectrumFall"));
    m_autoMoodEnabled = settings.value(QStringLiteral("autoMood"), true).toBool();

    ThemePalette def = presetClassicNeon();
    m_menuPalette = def;
    m_gamePalette = def;
    qApp->setStyleSheet(generateQss(def));
}

ThemePalette ThemeManager::gamePalette() const { return m_gamePalette; }
ThemePalette ThemeManager::menuPalette() const { return m_menuPalette; }
bool ThemeManager::autoMoodEnabled() const { return m_autoMoodEnabled; }

void ThemeManager::setAutoMoodEnabled(bool enabled)
{
    m_autoMoodEnabled = enabled;
    QSettings settings(QStringLiteral("SpectrumFall"), QStringLiteral("SpectrumFall"));
    settings.setValue(QStringLiteral("autoMood"), enabled);
    if (!enabled) {
        m_gamePalette = m_menuPalette;
        m_manualGameOverride = false;
        emit gameThemeChanged(m_gamePalette);
    }
}

bool ThemeManager::manualGameOverride() const { return m_manualGameOverride; }

void ThemeManager::setManualGameOverride(bool override)
{
    m_manualGameOverride = override;
}

void ThemeManager::clearManualGameOverride()
{
    m_manualGameOverride = false;
}

QString ThemeManager::moodName(Mood mood) const
{
    switch (mood) {
    case Mood::Energetic:   return QStringLiteral("激昂");
    case Mood::Cheerful:    return QStringLiteral("欢快");
    case Mood::Calm:        return QStringLiteral("舒缓");
    case Mood::Melancholic: return QStringLiteral("低沉");
    case Mood::Default:     return QStringLiteral("通用");
    }
    return QStringLiteral("通用");
}

QVector<ThemePalette> ThemeManager::allPresets() const
{
    QVector<ThemePalette> v;
    v << presetNeonCyber() << presetVibrantSunset()
      << presetWarmAmber() << presetDeepOcean() << presetClassicNeon();
    return v;
}

QString ThemeManager::hexToRgba(const QString& hex, int alpha) const
{
    QColor c(hex);
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

void ThemeManager::applyMenuTheme(Mood mood)
{
    ThemePalette p;
    switch (mood) {
    case Mood::Energetic:   p = presetNeonCyber(); break;
    case Mood::Cheerful:    p = presetVibrantSunset(); break;
    case Mood::Calm:        p = presetWarmAmber(); break;
    case Mood::Melancholic: p = presetDeepOcean(); break;
    case Mood::Default:     p = presetClassicNeon(); break;
    }
    applyMenuTheme(p);
}

void ThemeManager::applyMenuTheme(const ThemePalette& p)
{
    m_menuPalette = p;
    qApp->setStyleSheet(generateQss(p));
    emit menuThemeChanged(p);
    if (!m_autoMoodEnabled) {
        m_gamePalette = p;
        emit gameThemeChanged(m_gamePalette);
    }
}

void ThemeManager::applyGameTheme(Mood mood)
{
    ThemePalette p;
    switch (mood) {
    case Mood::Energetic:   p = presetNeonCyber(); break;
    case Mood::Cheerful:    p = presetVibrantSunset(); break;
    case Mood::Calm:        p = presetWarmAmber(); break;
    case Mood::Melancholic: p = presetDeepOcean(); break;
    case Mood::Default:     p = presetClassicNeon(); break;
    }
    applyGameTheme(p);
}

void ThemeManager::applyGameTheme(const ThemePalette& p)
{
    m_manualGameOverride = false;
    m_gamePalette = p;
    emit gameThemeChanged(m_gamePalette);
}

void ThemeManager::applyTheme(Mood mood)
{
    applyMenuTheme(mood);
    applyGameTheme(mood);
    m_manualGameOverride = true;
}

void ThemeManager::applyTheme(const ThemePalette& p)
{
    applyMenuTheme(p);
    applyGameTheme(p);
    m_manualGameOverride = true;
}

// -- QSS generation: replace hex + rgba --
QString ThemeManager::generateQss(const ThemePalette& p) const
{
    QString qss = m_baseQss;
    auto rep = [&](const QString& from, const QString& to) {
        qss.replace(from, to);
    };

    // primary alpha
    rep("rgba(0,255,136,12)",  hexToRgba(p.primary, 12));
    rep("rgba(0,255,136,45)",  hexToRgba(p.primary, 45));
    rep("rgba(0,255,136,60)",  hexToRgba(p.primary, 60));
    rep("rgba(0,255,136,90)",  hexToRgba(p.primary, 90));
    rep("rgba(0,255,136,70)",  hexToRgba(p.primary, 70));

    // tertiary alpha
    rep("rgba(139,233,253,10)", hexToRgba(p.actHoverBot, 10));
    rep("rgba(139,233,253,35)", hexToRgba(p.actHoverBot, 35));

    // secondary alpha
    rep("rgba(189,147,249,5)",  hexToRgba(p.secondary, 5));
    rep("rgba(189,147,249,25)", hexToRgba(p.secondary, 25));
    rep("rgba(189,147,249,50)", hexToRgba(p.secondary, 50));
    rep("rgba(189,147,249,55)", hexToRgba(p.secondary, 55));

    // surface alpha
    rep("rgba(15,52,96,200)",  hexToRgba(p.surface, 200));
    rep("rgba(20,80,140,230)", hexToRgba(p.surfaceHover, 230));

    // hex replacements (longest first to avoid substring issues)
    struct HexMap { const char* from; const QString* to; };
    HexMap map[] = {
        {"#bd93f9", &p.secondary},
        {"#d4b8ff", &p.secondaryLight},
        {"#c8d8e8", &p.btnText},
        {"#a070e0", &p.secondaryDark},
        {"#284878", &p.btnHoverTop},
        {"#2a5a9e", &p.actHoverTop},
        {"#1e3560", &p.btnBorder},
        {"#1c3858", &p.btnHoverBot},
        {"#1c2e50", &p.btnGradTop},
        {"#1a4a80", &p.actHoverBot},
        {"#1a3a6e", &p.actGradTop},
        {"#1a2a4a", &p.surfaceHover},
        {"#1a1a2e", &p.bg},
        {"#16213e", &p.surface},
        {"#141a30", &p.btnDisabledBd},
        {"#131d35", &p.btnGradBot},
        {"#10162a", &p.btnDisabledBg},
        {"#0f3460", &p.surfaceBorder},
        {"#0f2a52", &p.actGradBot},
        {"#0c1628", &p.btnPressed},
        {"#0a1a3a", &p.actPressed},
        {"#00ff88", &p.primary},
        {"#00cc77", &p.primaryMid},
        {"#00cc6a", &p.primaryDark},
        {"#eeffee", &p.hoverText},
        {"#e0e0e0", &p.text},
        {"#666666", &p.dimText},
        {"#3a3a50", &p.disabledText},
    };
    for (const auto& m : map)
        qss.replace(QLatin1String(m.from), *m.to);
    return qss;
}
ThemePalette ThemeManager::presetNeonCyber()
{
    ThemePalette p;
    p.name = QString::fromUtf8("éè¹èµå");
    p.bg="#0a0a1e";
    p.bgDeep="#050510";
    p.surface="#10183a";
    p.surfaceBorder="#1a2860";
    p.surfaceHover="#1a2850";
    p.text="#e0e8ff";
    p.btnText="#b0c8e8";
    p.hoverText="#e0ffff";
    p.dimText="#555570";
    p.disabledText="#2a2a40";
    p.btnGradTop="#1a2860";
    p.btnGradBot="#0e1840";
    p.btnHoverTop="#2848a0";
    p.btnHoverBot="#1a3870";
    p.btnPressed="#080e20";
    p.btnBorder="#1a3870";
    p.btnDisabledBg="#0a0e1a";
    p.btnDisabledBd="#101420";
    p.actGradTop="#182868";
    p.actGradBot="#0e1848";
    p.actHoverTop="#2858b8";
    p.actHoverBot="#1a4890";
    p.actPressed="#060e28";
    p.primary="#00f0ff";
    p.primaryDark="#00c8d8";
    p.primaryMid="#00d0e0";
    p.secondary="#ff2d95";
    p.secondaryLight="#ff6db8";
    p.secondaryDark="#d01878";
    p.glowPrimary="#00f0ff";
    p.glowSecondary="#ff2d95";
    p.visBg="#0a0a1e";
    p.visBgDeep="#050510";
    p.judgeLine="#00f0ff";
    p.laneColors[0]=QColor(0,240,255);
    p.laneColors[1]=QColor(0,240,255);
    p.laneColors[2]=QColor(255,45,149);
    p.laneColors[3]=QColor(139,233,253);
    p.laneColors[4]=QColor(255,100,200);
    p.laneColors[5]=QColor(255,180,50);
    return p;
}
ThemePalette ThemeManager::presetVibrantSunset()
{
    ThemePalette p;
    p.name = QString::fromUtf8("æ´»åæ©å½©");
    p.bg="#1a1218";
    p.bgDeep="#0e0810";
    p.surface="#2a1a28";
    p.surfaceBorder="#3a2040";
    p.surfaceHover="#352035";
    p.text="#ffe8d0";
    p.btnText="#e0c8b0";
    p.hoverText="#fff0e0";
    p.dimText="#665555";
    p.disabledText="#3a2a2a";
    p.btnGradTop="#3a2030";
    p.btnGradBot="#281420";
    p.btnHoverTop="#4a3040";
    p.btnHoverBot="#382030";
    p.btnPressed="#1a0e14";
    p.btnBorder="#4a2840";
    p.btnDisabledBg="#1a1018";
    p.btnDisabledBd="#2a1820";
    p.actGradTop="#3a2038";
    p.actGradBot="#281428";
    p.actHoverTop="#4a3858";
    p.actHoverBot="#382848";
    p.actPressed="#180e18";
    p.primary="#ff9f43";
    p.primaryDark="#e08830";
    p.primaryMid="#f09040";
    p.secondary="#ff6b9d";
    p.secondaryLight="#ff9dc0";
    p.secondaryDark="#d05080";
    p.glowPrimary="#ff9f43";
    p.glowSecondary="#ff6b9d";
    p.visBg="#1a1218";
    p.visBgDeep="#0e0810";
    p.judgeLine="#ff9f43";
    p.laneColors[0]=QColor(255,159,67);
    p.laneColors[1]=QColor(255,159,67);
    p.laneColors[2]=QColor(255,107,157);
    p.laneColors[3]=QColor(255,200,100);
    p.laneColors[4]=QColor(255,140,180);
    p.laneColors[5]=QColor(255,220,80);
    return p;
}
ThemePalette ThemeManager::presetWarmAmber()
{
    ThemePalette p;
    p.name = QString::fromUtf8("æåç¥ç");
    p.bg="#1a1610";
    p.bgDeep="#0e0c08";
    p.surface="#28201a";
    p.surfaceBorder="#3a3020";
    p.surfaceHover="#302820";
    p.text="#e8d5b7";
    p.btnText="#d0c0a0";
    p.hoverText="#f0e8d0";
    p.dimText="#665540";
    p.disabledText="#3a3020";
    p.btnGradTop="#302818";
    p.btnGradBot="#201810";
    p.btnHoverTop="#403828";
    p.btnHoverBot="#302818";
    p.btnPressed="#181008";
    p.btnBorder="#403020";
    p.btnDisabledBg="#181410";
    p.btnDisabledBd="#282018";
    p.actGradTop="#302820";
    p.actGradBot="#201810";
    p.actHoverTop="#403830";
    p.actHoverBot="#302820";
    p.actPressed="#181008";
    p.primary="#f0c040";
    p.primaryDark="#d0a030";
    p.primaryMid="#e0b038";
    p.secondary="#e09050";
    p.secondaryLight="#f0b080";
    p.secondaryDark="#c07030";
    p.glowPrimary="#f0c040";
    p.glowSecondary="#e09050";
    p.visBg="#1a1610";
    p.visBgDeep="#0e0c08";
    p.judgeLine="#f0c040";
    p.laneColors[0]=QColor(240,192,64);
    p.laneColors[1]=QColor(240,192,64);
    p.laneColors[2]=QColor(224,144,80);
    p.laneColors[3]=QColor(240,210,120);
    p.laneColors[4]=QColor(240,160,100);
    p.laneColors[5]=QColor(240,220,80);
    return p;
}
ThemePalette ThemeManager::presetDeepOcean()
{
    ThemePalette p;
    p.name = QString::fromUtf8("æ·±æµ·èè°");
    p.bg="#0e1218";
    p.bgDeep="#060810";
    p.surface="#141e2a";
    p.surfaceBorder="#1e3040";
    p.surfaceHover="#1a2838";
    p.text="#b0c0d0";
    p.btnText="#90a0b0";
    p.hoverText="#c0d8e8";
    p.dimText="#506070";
    p.disabledText="#283848";
    p.btnGradTop="#1a2838";
    p.btnGradBot="#101828";
    p.btnHoverTop="#283848";
    p.btnHoverBot="#1a2838";
    p.btnPressed="#0a1018";
    p.btnBorder="#1e3040";
    p.btnDisabledBg="#0e1218";
    p.btnDisabledBd="#141e2a";
    p.actGradTop="#1a2840";
    p.actGradBot="#101830";
    p.actHoverTop="#283858";
    p.actHoverBot="#1a2848";
    p.actPressed="#0a1020";
    p.primary="#4a8fcf";
    p.primaryDark="#3870a8";
    p.primaryMid="#4080b8";
    p.secondary="#8899aa";
    p.secondaryLight="#a0b8cc";
    p.secondaryDark="#607080";
    p.glowPrimary="#4a8fcf";
    p.glowSecondary="#8899aa";
    p.visBg="#0e1218";
    p.visBgDeep="#060810";
    p.judgeLine="#4a8fcf";
    p.laneColors[0]=QColor(74,143,207);
    p.laneColors[1]=QColor(74,143,207);
    p.laneColors[2]=QColor(136,153,170);
    p.laneColors[3]=QColor(100,180,220);
    p.laneColors[4]=QColor(160,140,200);
    p.laneColors[5]=QColor(120,200,180);
    return p;
}
ThemePalette ThemeManager::presetClassicNeon()
{
    ThemePalette p;
    p.name = QString::fromUtf8("ç»å¸ç»¿ç´«");
    p.bg="#1a1a2e";
    p.bgDeep="#0a0a1e";
    p.surface="#16213e";
    p.surfaceBorder="#0f3460";
    p.surfaceHover="#1a2a4a";
    p.text="#e0e0e0";
    p.btnText="#c8d8e8";
    p.hoverText="#eeffee";
    p.dimText="#666666";
    p.disabledText="#3a3a50";
    p.btnGradTop="#1c2e50";
    p.btnGradBot="#131d35";
    p.btnHoverTop="#284878";
    p.btnHoverBot="#1c3858";
    p.btnPressed="#0c1628";
    p.btnBorder="#1e3560";
    p.btnDisabledBg="#10162a";
    p.btnDisabledBd="#141a30";
    p.actGradTop="#1a3a6e";
    p.actGradBot="#0f2a52";
    p.actHoverTop="#2a5a9e";
    p.actHoverBot="#1a4a80";
    p.actPressed="#0a1a3a";
    p.primary="#00ff88";
    p.primaryDark="#00cc6a";
    p.primaryMid="#00cc77";
    p.secondary="#bd93f9";
    p.secondaryLight="#d4b8ff";
    p.secondaryDark="#a070e0";
    p.glowPrimary="#00ff88";
    p.glowSecondary="#bd93f9";
    p.visBg="#1a1a2e";
    p.visBgDeep="#0a0a1e";
    p.judgeLine="#00ff88";
    p.laneColors[0]=QColor(0,255,136);
    p.laneColors[1]=QColor(0,255,136);
    p.laneColors[2]=QColor(189,147,249);
    p.laneColors[3]=QColor(139,233,253);
    p.laneColors[4]=QColor(255,121,198);
    p.laneColors[5]=QColor(255,180,50);
    return p;
}