#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <QVector>

/// 主题调色板：定义 UI 全局颜色
struct ThemePalette {
    QString name;

    // 背景
    QString bg;           // #1a1a2e - 全局深色背景
    QString bgDeep;       // #0a0a1e - 更深的背景（游戏/瀑布图）

    // 面板
    QString surface;      // #16213e - 卡片/列表/面板背景
    QString surfaceBorder;// #0f3460 - 面板边框
    QString surfaceHover; // #1a2a4a - 列表悬停背景

    // 文字
    QString text;         // #e0e0e0 - 主文字
    QString btnText;      // #c8d8e8 - 按钮默认文字
    QString hoverText;    // #eeffee - 按钮悬停文字
    QString dimText;      // #666666 - 禁用/暗淡文字
    QString disabledText; // #3a3a50 - 禁用按钮文字

    // 按钮
    QString btnGradTop;   // #1c2e50 - 按钮渐变顶部
    QString btnGradBot;   // #131d35 - 按钮渐变底部
    QString btnHoverTop;  // #284878 - 按钮悬停渐变顶部
    QString btnHoverBot;  // #1c3858 - 按钮悬停渐变底部
    QString btnPressed;   // #0c1628 - 按钮按下背景
    QString btnBorder;    // #1e3560 - 按钮默认边框
    QString btnDisabledBg;// #10162a - 禁用按钮背景
    QString btnDisabledBd;// #141a30 - 禁用按钮边框

    // 动作按钮
    QString actGradTop;   // #1a3a6e
    QString actGradBot;   // #0f2a52
    QString actHoverTop;  // #2a5a9e
    QString actHoverBot;  // #1a4a80
    QString actPressed;   // #0a1a3a

    // 强调色
    QString primary;      // #00ff88 - 主强调（绿）
    QString primaryDark;  // #00cc6a - 按下状态
    QString primaryMid;   // #00cc77 - 进度条

    // 副强调色
    QString secondary;      // #bd93f9 - 副强调（紫）
    QString secondaryLight; // #d4b8ff
    QString secondaryDark;  // #a070e0

    // 辉光
    QString glowPrimary;    // #00ff88
    QString glowSecondary;  // #bd93f9

    // 可视化背景
    QString visBg;          // #1a1a2e
    QString visBgDeep;      // #0a0a1e

    // 判定线 & 游戏
    QString judgeLine;      // #00ff88

    // 轨道颜色 (6 个)
    QColor laneColors[6];
};

/// 情绪分类枚举
enum class Mood {
    Energetic,   // 激昂 - 霓虹赛博
    Cheerful,    // 欢快 - 活力橙彩
    Calm,        // 舒缓 - 暖光琥珀
    Melancholic, // 低沉 - 深海蓝调
    Default      // 通用 - 经典绿紫
};

/// 主题管理器（单例）：菜单主题（QSS）和游戏主题（绘制色）独立
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager* instance();

    // ── 菜单主题（控制 QSS 样式）──
    void applyMenuTheme(Mood mood);
    void applyMenuTheme(const ThemePalette& palette);

    // ── 游戏主题（控制可视化 / GameWidget 绘制色）──
    void applyGameTheme(Mood mood);
    void applyGameTheme(const ThemePalette& palette);
    ThemePalette gamePalette() const;
    ThemePalette menuPalette() const;

    // ── 自动切换开关 ──
    bool autoMoodEnabled() const;
    void setAutoMoodEnabled(bool enabled);

    // ── 手动覆盖标志（用户手动选了主题后阻止自动分类覆盖）──
    bool manualGameOverride() const;
    void setManualGameOverride(bool override);
    void clearManualGameOverride();

    // ── 便捷方法（同时设置菜单+游戏主题）──
    void applyTheme(Mood mood);
    void applyTheme(const ThemePalette& palette);

    QVector<ThemePalette> allPresets() const;
    QString moodName(Mood mood) const;

    static ThemePalette presetNeonCyber();
    static ThemePalette presetVibrantSunset();
    static ThemePalette presetWarmAmber();
    static ThemePalette presetDeepOcean();
    static ThemePalette presetClassicNeon();

signals:
    void gameThemeChanged(const ThemePalette& palette);
    void menuThemeChanged(const ThemePalette& palette);

private:
    explicit ThemeManager(QObject* parent = nullptr);

    QString generateQss(const ThemePalette& p) const;
    QString hexToRgba(const QString& hex, int alpha) const;

    static ThemeManager* s_instance;

    ThemePalette m_menuPalette;   ///< 菜单主题（生成 QSS）
    ThemePalette m_gamePalette;   ///< 游戏主题（绘制用）
    bool m_autoMoodEnabled = true;
    bool m_manualGameOverride = false;
    Mood m_currentMood = Mood::Default;
    QString m_baseQss;
};
