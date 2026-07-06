#include "ThemeEditorWidget.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QColorDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QStandardPaths>
#include <QDir>
#include <QAbstractButton>
#include <QPainter>
#include <QMouseEvent>

// ── 滑动开关 ──────────────────────────────────────────────────────
class ToggleSwitch : public QAbstractButton
{
public:
    explicit ToggleSwitch(QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(48, 26);
    }

    QSize sizeHint() const override { return QSize(48, 26); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        int w = width(), h = height(), r = h / 2;

        // track
        QColor trackColor = isChecked() ? QColor(0, 255, 136) : QColor(60, 60, 80);
        p.setPen(Qt::NoPen);
        p.setBrush(trackColor);
        p.drawRoundedRect(0, 0, w, h, r, r);

        // knob
        int knobSize = h - 4;
        int knobX = isChecked() ? (w - h + 2) : 2;
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(knobX + knobSize / 2.0, h / 2.0), knobSize / 2.0, knobSize / 2.0);
    }
};

ThemeEditorWidget::ThemeEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    // 自定义预设文件路径
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));
    m_presetFilePath = dataDir + QStringLiteral("/custom_themes.json");

    loadCustomPresets();
    setupUI();
    refreshCustomPresetButtons();

    // 初始选中通用主题
    onPresetClicked(4);
}

// ── 持久化 ──────────────────────────────────────────────────────
void ThemeEditorWidget::loadCustomPresets()
{
    m_customPresets.clear();
    QFile file(m_presetFilePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return;

    QJsonArray arr = doc.object().value(QStringLiteral("presets")).toArray();
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        CustomPreset cp;
        cp.name = obj.value(QStringLiteral("name")).toString();
        QJsonObject colors = obj.value(QStringLiteral("colors")).toObject();
        for (auto it = colors.begin(); it != colors.end(); ++it)
            cp.colors[it.key()] = it.value().toString();
        m_customPresets.append(cp);
    }
}

void ThemeEditorWidget::saveCustomPresets()
{
    QJsonArray arr;
    for (const CustomPreset& cp : m_customPresets) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = cp.name;
        QJsonObject colors;
        for (auto it = cp.colors.begin(); it != cp.colors.end(); ++it)
            colors[it.key()] = it.value();
        obj[QStringLiteral("colors")] = colors;
        arr.append(obj);
    }
    QJsonObject root;
    root[QStringLiteral("presets")] = arr;

    QFile file(m_presetFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
    }
}

// ── UI 构建 ──────────────────────────────────────────────────────
void ThemeEditorWidget::setupUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 30, 40, 30);
    root->setSpacing(20);

    // 标题
    auto* title = new QLabel(QStringLiteral("主题编辑器"), this);
    title->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: bold; color: #e0e0e0;"));
    root->addWidget(title);

    // 当前主题指示
    m_currentMoodLabel = new QLabel(this);
    m_currentMoodLabel->setStyleSheet(QStringLiteral("font-size: 15px; color: #888;"));
    root->addWidget(m_currentMoodLabel);

    // 自动切换开关
    auto* toggleRow = new QHBoxLayout();
    toggleRow->setSpacing(10);
    m_autoMoodToggle = new ToggleSwitch(this);
    m_autoMoodToggle->setChecked(ThemeManager::instance()->autoMoodEnabled());
    toggleRow->addWidget(m_autoMoodToggle);
    auto* toggleLabel = new QLabel(QStringLiteral("根据歌曲情绪自动切换游戏内主题"), this);
    toggleLabel->setStyleSheet(QStringLiteral("font-size: 14px; color: #ccc;"));
    toggleRow->addWidget(toggleLabel);
    toggleRow->addStretch();
    connect(m_autoMoodToggle, &ToggleSwitch::toggled, this, &ThemeEditorWidget::onAutoMoodToggled);
    root->addLayout(toggleRow);

    // 内置预设按钮行
    auto* presetRow = new QHBoxLayout();
    presetRow->setSpacing(12);

    QStringList names = {
        QStringLiteral("激昂"),
        QStringLiteral("欢快"),
        QStringLiteral("舒缓"),
        QStringLiteral("低沉"),
        QStringLiteral("通用")
    };

    for (int i = 0; i < names.size(); ++i) {
        auto* btn = new QPushButton(names[i], this);
        btn->setMinimumSize(90, 38);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, i]() { onPresetClicked(i); });
        presetRow->addWidget(btn);
        m_presetBtns.append(btn);
    }

    m_customBtn = new QPushButton(QStringLiteral("自定义"), this);
    m_customBtn->setMinimumSize(90, 38);
    m_customBtn->setCursor(Qt::PointingHandCursor);
    connect(m_customBtn, &QPushButton::clicked, this, &ThemeEditorWidget::onCustomClicked);
    presetRow->addWidget(m_customBtn);
    presetRow->addStretch();
    root->addLayout(presetRow);

    // 自定义预设按钮行（动态填充）
    m_customPresetRow = new QWidget(this);
    m_customPresetLayout = new QHBoxLayout(m_customPresetRow);
    m_customPresetLayout->setContentsMargins(0, 0, 0, 0);
    m_customPresetLayout->setSpacing(12);
    m_customPresetLayout->addStretch();
    root->addWidget(m_customPresetRow);

    // 自定义面板（默认隐藏）
    m_customPanel = new QWidget(this);
    auto* customLayout = new QVBoxLayout(m_customPanel);
    customLayout->setSpacing(16);

    auto* gridLabel = new QLabel(QStringLiteral("调色板（点击色块选择颜色）"), m_customPanel);
    gridLabel->setStyleSheet(QStringLiteral("font-size: 14px; color: #aaa;"));
    customLayout->addWidget(gridLabel);

    auto* grid = new QGridLayout();
    grid->setSpacing(10);

    struct ColorDef { const char* key; const char* label; const char* defaultHex; };
    ColorDef defs[] = {
        {"primary",      "主强调色",  "#00ff88"},
        {"secondary",    "副强调色",  "#bd93f9"},
        {"bg",           "背景色",      "#1a1a2e"},
        {"bgDeep",       "深背景色",    "#0a0a1e"},
        {"surface",      "面板背景",    "#16213e"},
        {"text",         "文字颜色",    "#e0e0e0"},
        {"judgeLine",    "判定线色",    "#00ff88"},
        {"glowPrimary",  "主辉光色",  "#00ff88"},
        {"glowSecondary","副辉光色", "#bd93f9"},
    };

    for (int row = 0; row < 9; ++row) {
        auto* lbl = new QLabel(QString::fromUtf8(defs[row].label), m_customPanel);
        lbl->setStyleSheet(QStringLiteral("font-size: 14px; color: #ccc;"));
        grid->addWidget(lbl, row, 0);

        auto* swatch = new QPushButton(m_customPanel);
        swatch->setFixedSize(60, 28);
        swatch->setCursor(Qt::PointingHandCursor);
        QColor dc(QString::fromUtf8(defs[row].defaultHex));
        m_customColors[QString::fromUtf8(defs[row].key)] = dc;
        swatch->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; border: 1px solid #444; border-radius: 4px; }")
            .arg(dc.name()));
        QString key = QString::fromUtf8(defs[row].key);
        connect(swatch, &QPushButton::clicked, this, [this, key]() { onColorPickClicked(key); });
        m_swatchBtns[key] = swatch;
        grid->addWidget(swatch, row, 1);
    }
    customLayout->addLayout(grid);

    // 操作按钮行
    auto* actionRow = new QHBoxLayout();
    actionRow->setSpacing(12);

    m_applyBtn = new QPushButton(QStringLiteral("应用主题"), m_customPanel);
    m_applyBtn->setMinimumSize(120, 38);
    m_applyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_applyBtn, &QPushButton::clicked, this, &ThemeEditorWidget::onApplyCustom);
    actionRow->addWidget(m_applyBtn);

    m_savePresetBtn = new QPushButton(QStringLiteral("保存为预设"), m_customPanel);
    m_savePresetBtn->setMinimumSize(120, 38);
    m_savePresetBtn->setCursor(Qt::PointingHandCursor);
    connect(m_savePresetBtn, &QPushButton::clicked, this, &ThemeEditorWidget::onSavePresetClicked);
    actionRow->addWidget(m_savePresetBtn);

    actionRow->addStretch();
    customLayout->addLayout(actionRow);

    m_customPanel->setVisible(false);
    root->addWidget(m_customPanel);

    root->addStretch();

    // 底部：重置 / 导出 / 导入 / 返回
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(16);

    m_resetBtn = new QPushButton(QStringLiteral("重置预设"), this);
    m_resetBtn->setMinimumSize(110, 36);
    m_resetBtn->setCursor(Qt::PointingHandCursor);
    connect(m_resetBtn, &QPushButton::clicked, this, &ThemeEditorWidget::onResetClicked);
    bottomRow->addWidget(m_resetBtn);

    m_exportBtn = new QPushButton(QStringLiteral("导出主题"), this);
    m_exportBtn->setMinimumSize(110, 36);
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    connect(m_exportBtn, &QPushButton::clicked, this, &ThemeEditorWidget::onExportClicked);
    bottomRow->addWidget(m_exportBtn);

    m_importBtn = new QPushButton(QStringLiteral("导入主题"), this);
    m_importBtn->setMinimumSize(110, 36);
    m_importBtn->setCursor(Qt::PointingHandCursor);
    connect(m_importBtn, &QPushButton::clicked, this, &ThemeEditorWidget::onImportClicked);
    bottomRow->addWidget(m_importBtn);

    bottomRow->addStretch();

    m_backBtn = new QPushButton(QStringLiteral("返回"), this);
    m_backBtn->setObjectName(QStringLiteral("backButton"));
    m_backBtn->setMinimumSize(100, 36);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    connect(m_backBtn, &QPushButton::clicked, this, &ThemeEditorWidget::backRequested);
    bottomRow->addWidget(m_backBtn);

    root->addLayout(bottomRow);
}

// ── 预设切换 ──────────────────────────────────────────────────────
void ThemeEditorWidget::onPresetClicked(int index)
{
    ThemeManager::instance()->applyTheme(static_cast<Mood>(index));
    updateActiveButton(index);
    showCustomPanel(false);
    m_currentMoodLabel->setText(
        QStringLiteral("当前主题：") + ThemeManager::instance()->moodName(static_cast<Mood>(index)));
}

void ThemeEditorWidget::onCustomClicked()
{
    updateActiveButton(5);
    showCustomPanel(true);
    m_currentMoodLabel->setText(QStringLiteral("当前主题：自定义"));
}

void ThemeEditorWidget::updateActiveButton(int index)
{
    m_activeIndex = index;
    QString active = QStringLiteral("QPushButton { background-color: #00ff88; color: #111; border-radius: 4px; font-weight: bold; }");
    for (int i = 0; i < m_presetBtns.size(); ++i)
        m_presetBtns[i]->setStyleSheet(i == index ? active : QString());
    m_customBtn->setStyleSheet(index == 5 ? active : QString());
    for (int i = 0; i < m_savedPresetBtns.size(); ++i)
        m_savedPresetBtns[i]->setStyleSheet(QString());
}

void ThemeEditorWidget::showCustomPanel(bool visible)
{
    m_customPanel->setVisible(visible);
}

// ── 自定义预设按钮刷新 ──────────────────────────────────────────────
void ThemeEditorWidget::refreshCustomPresetButtons()
{
    // 清除旧按钮
    for (auto* btn : m_savedPresetBtns) {
        m_customPresetLayout->removeWidget(btn);
        delete btn;
    }
    m_savedPresetBtns.clear();

    for (int i = 0; i < m_customPresets.size(); ++i) {
        auto* btn = new QPushButton(m_customPresets[i].name, m_customPresetRow);
        btn->setMinimumSize(90, 38);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(QStringLiteral("右键点击删除此预设"));
        connect(btn, &QPushButton::clicked, this, [this, i]() { onSavedPresetClicked(i); });
        // 右键删除
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, this, [this, btn, i](const QPoint&) {
            QMenu menu;
            menu.addAction(QStringLiteral("删除此预设"), this, [this, i]() { onDeletePreset(i); });
            menu.exec(btn->mapToGlobal(QPoint(0, btn->height())));
        });
        // 插入到 stretch 之前
        int insertPos = m_customPresetLayout->count() - 1;
        m_customPresetLayout->insertWidget(insertPos, btn);
        m_savedPresetBtns.append(btn);
    }
}

// ── 自定义预设点击 ──────────────────────────────────────────────
void ThemeEditorWidget::onSavedPresetClicked(int index)
{
    if (index < 0 || index >= m_customPresets.size()) return;

    ThemePalette p = buildPaletteFromColors(m_customPresets[index].colors);
    p.name = m_customPresets[index].name;
    ThemeManager::instance()->applyTheme(p);

    // 高亮对应按钮
    m_activeIndex = 100 + index;
    QString active = QStringLiteral("QPushButton { background-color: #00ff88; color: #111; border-radius: 4px; font-weight: bold; }");
    for (int i = 0; i < m_presetBtns.size(); ++i) m_presetBtns[i]->setStyleSheet(QString());
    m_customBtn->setStyleSheet(QString());
    for (int i = 0; i < m_savedPresetBtns.size(); ++i)
        m_savedPresetBtns[i]->setStyleSheet(i == index ? active : QString());

    showCustomPanel(false);
    m_currentMoodLabel->setText(QStringLiteral("当前主题：") + m_customPresets[index].name);
}

void ThemeEditorWidget::onDeletePreset(int index)
{
    if (index < 0 || index >= m_customPresets.size()) return;
    int ret = QMessageBox::question(this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定删除预设「%1」？").arg(m_customPresets[index].name));
    if (ret != QMessageBox::Yes) return;
    m_customPresets.removeAt(index);
    saveCustomPresets();
    refreshCustomPresetButtons();
}

// ── 颜色选择 ──────────────────────────────────────────────────────
void ThemeEditorWidget::onColorPickClicked(const QString& key)
{
    QColor current = m_customColors.value(key, Qt::white);
    QColor chosen = QColorDialog::getColor(current, this, QStringLiteral("选择颜色"));
    if (!chosen.isValid()) return;

    m_customColors[key] = chosen;
    QPushButton* swatch = m_swatchBtns.value(key);
    if (swatch) {
        swatch->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; border: 1px solid #444; border-radius: 4px; }")
            .arg(chosen.name()));
    }
}

// ── 从颜色映射构建完整调色板 ──────────────────────────────────────
ThemePalette ThemeEditorWidget::buildPaletteFromColors(const QMap<QString, QString>& colors) const
{
    ThemePalette p = ThemeManager::presetClassicNeon();
    auto get = [&](const QString& key) -> QString {
        return colors.value(key, QString());
    };
    QString v;
    if (!(v = get("primary")).isEmpty())     { p.primary = v; p.primaryMid = v; p.primaryDark = v; }
    if (!(v = get("secondary")).isEmpty())   p.secondary = v;
    if (!(v = get("bg")).isEmpty())          { p.bg = v; p.visBg = v; }
    if (!(v = get("bgDeep")).isEmpty())      { p.bgDeep = v; p.visBgDeep = v; }
    if (!(v = get("surface")).isEmpty())     p.surface = v;
    if (!(v = get("text")).isEmpty())        p.text = v;
    if (!(v = get("judgeLine")).isEmpty())   p.judgeLine = v;
    if (!(v = get("glowPrimary")).isEmpty()) p.glowPrimary = v;
    if (!(v = get("glowSecondary")).isEmpty()) p.glowSecondary = v;

    // 轨道色跟随主/副强调
    QColor c1(p.primary), c2(p.secondary);
    p.laneColors[0] = c1;
    p.laneColors[1] = c1;
    p.laneColors[2] = c2;
    return p;
}

// ── 应用自定义主题 ────────────────────────────────────────────────
void ThemeEditorWidget::onApplyCustom()
{
    QMap<QString, QString> cmap;
    for (auto it = m_customColors.begin(); it != m_customColors.end(); ++it)
        cmap[it.key()] = it.value().name();

    ThemePalette p = buildPaletteFromColors(cmap);
    p.name = QStringLiteral("自定义");
    ThemeManager::instance()->applyTheme(p);

    updateActiveButton(5);
    m_currentMoodLabel->setText(QStringLiteral("当前主题：自定义"));
}

// ── 保存为预设 ──────────────────────────────────────────────────
void ThemeEditorWidget::onSavePresetClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this,
        QStringLiteral("保存预设"),
        QStringLiteral("请输入预设名称："),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    name = name.trimmed();

    // 检查重名
    for (int i = 0; i < m_customPresets.size(); ++i) {
        if (m_customPresets[i].name == name) {
            int ret = QMessageBox::question(this,
                QStringLiteral("预设已存在"),
                QStringLiteral("预设「%1」已存在，是否覆盖？").arg(name));
            if (ret == QMessageBox::Yes) {
                // 覆盖
                for (auto it = m_customColors.begin(); it != m_customColors.end(); ++it)
                    m_customPresets[i].colors[it.key()] = it.value().name();
                saveCustomPresets();
                refreshCustomPresetButtons();
                QMessageBox::information(this, QStringLiteral("保存成功"),
                    QStringLiteral("预设「%1」已更新").arg(name));
                return;
            }
            return;
        }
    }

    CustomPreset cp;
    cp.name = name;
    for (auto it = m_customColors.begin(); it != m_customColors.end(); ++it)
        cp.colors[it.key()] = it.value().name();
    m_customPresets.append(cp);
    saveCustomPresets();
    refreshCustomPresetButtons();

    QMessageBox::information(this, QStringLiteral("保存成功"),
        QStringLiteral("预设「%1」已保存").arg(name));
}

// ── 重置预设 ──────────────────────────────────────────────────────
void ThemeEditorWidget::onResetClicked()
{
    if (m_customPresets.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("当前没有自定义预设可以重置"));
        return;
    }
    int ret = QMessageBox::warning(this,
        QStringLiteral("确认重置"),
        QStringLiteral("将删除所有 %1 个自定义预设，此操作不可撤销。确定继续？")
            .arg(m_customPresets.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    m_customPresets.clear();
    saveCustomPresets();
    refreshCustomPresetButtons();

    // 恢复为默认通用主题
    ThemeManager::instance()->applyTheme(Mood::Default);
    onPresetClicked(4);
    QMessageBox::information(this, QStringLiteral("重置完成"),
        QStringLiteral("所有自定义预设已删除"));
}

// ── 导出/导入 ────────────────────────────────────────────────────
void ThemeEditorWidget::onExportClicked()
{
    QString path = QFileDialog::getSaveFileName(this,
        QStringLiteral("导出主题"), QString(),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) return;

    QMap<QString, QString> cmap;
    for (auto it = m_customColors.begin(); it != m_customColors.end(); ++it)
        cmap[it.key()] = it.value().name();

    QJsonObject obj;
    for (auto it = cmap.begin(); it != cmap.end(); ++it)
        obj[it.key()] = it.value();
    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("colors")] = obj;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        QMessageBox::information(this, QStringLiteral("导出成功"),
            QStringLiteral("主题已保存到 %1").arg(path));
    } else {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
            QStringLiteral("无法写入文件"));
    }
}

void ThemeEditorWidget::onImportClicked()
{
    QString path = QFileDialog::getOpenFileName(this,
        QStringLiteral("导入主题"), QString(),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("无法读取文件"));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("文件格式错误"));
        return;
    }

    QJsonObject colorsObj = doc.object().value(QStringLiteral("colors")).toObject();
    for (auto it = colorsObj.begin(); it != colorsObj.end(); ++it) {
        QColor c(it.value().toString());
        if (c.isValid() && m_swatchBtns.contains(it.key())) {
            m_customColors[it.key()] = c;
            m_swatchBtns[it.key()]->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: %1; border: 1px solid #444; border-radius: 4px; }")
                .arg(c.name()));
        }
    }

    // 自动打开自定义面板让用户看到
    onCustomClicked();

    QMessageBox::information(this, QStringLiteral("\xe5\xaf\xbc\xe5\x85\xa5\xe6\x88\x90\xe5\x8a\x9f"),
        QStringLiteral("\xe5\xb7\xb2\xe5\x8a\xa0\xe8\xbd\xbd %1 \xe4\xb8\xaa\xe9\xa2\x9c\xe8\x89\xb2\xe9\x85\x8d\xe7\xbd\xae\xef\xbc\x8c\xe7\x82\xb9\xe5\x87\xbb\xe5\xba\x94\xe7\x94\xa8\xe4\xb8\xbb\xe9\xa2\x98\xe6\x88\x96\xe4\xbf\x9d\xe5\xad\x98\xe4\xb8\xba\xe9\xa2\x84\xe8\xae\xbe")
        .arg(colorsObj.size()));
}

// ── 自动切换开关 ──────────────────────────────────────────────────
void ThemeEditorWidget::onAutoMoodToggled(bool checked)
{
    ThemeManager::instance()->setAutoMoodEnabled(checked);
}

